#!/usr/bin/env python3
"""Read binary packets from Master or Slave Arduino Nano over Serial and format as JSON."""

from __future__ import annotations

import argparse
import json
import struct
import sys
import time
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from simulator.model import (
    ALARM_CODE_TO_STR,
    DeviceModel,
    GW_STATE_PACKET_SIZE,
    GW_SYNC_BYTE,
    crc16_ccitt,
    unpack_gw_state_packet,
)
from simulator.server import _load_pyserial, detect_serial_port

RS485_SYNC_BYTE = 0xAA
RS485_POLL_CMD_PREFIX = 0xB0
STATUS_FLAG_MOTION = 0x01
STATUS_FLAG_ERROR = 0x02
STATUS_FLAG_PANIC = 0x04
STATUS_FLAG_HB = 0x08

_RS485_STRUCT = struct.Struct("<BBBHBH")
RS485_PACKET_SIZE = _RS485_STRUCT.size

TRIGGER_REASON_NAMES = {
    0: "NONE",
    1: "PANIC",
    2: "MOTION",
    3: "DISCONNECT",
}

SECTOR_NAMES = {sec_id: name for sec_id, name, _, _ in DeviceModel.SECTOR_DEFINITIONS}


def unpack_rs485_packet(raw: bytes) -> dict | None:
    if len(raw) != RS485_PACKET_SIZE:
        return None
    sync, node_id, subnode_count, distance_cm, status_flags, expected_crc = _RS485_STRUCT.unpack(raw)
    if sync != RS485_SYNC_BYTE or not (1 <= node_id <= 9):
        return None
    if crc16_ccitt(raw[:6]) != expected_crc:
        return None
    return {
        "node_id": node_id,
        "subnode_count": subnode_count,
        "distance_cm": distance_cm,
        "status_flags": status_flags,
        "crc": expected_crc,
    }


def format_gw_state_json(parsed: dict, timestamp: str | None = None) -> dict:
    ts = timestamp or datetime.now().astimezone().isoformat(timespec="milliseconds")
    code = int(parsed["alarm_state"])
    trig_reason = int(parsed["trigger_reason"])
    trig_sector = int(parsed["trigger_sector"])
    return {
        "timestamp": ts,
        "packet_type": "gw_state_packet_t",
        "alarm_state": code,
        "alarm_state_name": ALARM_CODE_TO_STR.get(code, "UNKNOWN"),
        "arming_time_s": parsed["arming_time_s"],
        "grace_time_s": parsed["grace_time_s"],
        "trigger_reason": trig_reason,
        "trigger_reason_name": TRIGGER_REASON_NAMES.get(trig_reason, "UNKNOWN"),
        "trigger_sector": trig_sector,
        "trigger_sector_name": SECTOR_NAMES.get(trig_sector) if trig_sector else None,
        "sectors": [
            {
                "id": sec["id"],
                "name": SECTOR_NAMES.get(sec["id"], f"Sector {sec['id']}"),
                "enabled": sec["enabled"],
                "online": sec["online"],
                "motion": sec["motion"],
                "panic": sec["panic"],
                "distance_cm": sec["distance_cm"],
                "last_seen_ms": sec["last_seen_ms"],
            }
            for sec in parsed["sectors"]
        ],
    }


def format_rs485_json(parsed: dict, timestamp: str | None = None) -> dict:
    ts = timestamp or datetime.now().astimezone().isoformat(timespec="milliseconds")
    flags = int(parsed["status_flags"])
    node_id = int(parsed["node_id"])
    return {
        "timestamp": ts,
        "packet_type": "rs485_packet_t",
        "node_id": node_id,
        "sector_name": SECTOR_NAMES.get(node_id, f"Nodo {node_id}"),
        "subnode_count": parsed["subnode_count"],
        "distance_cm": parsed["distance_cm"],
        "status_flags": flags,
        "flags": {
            "heartbeat": bool(flags & STATUS_FLAG_HB),
            "motion": bool(flags & STATUS_FLAG_MOTION),
            "panic": bool(flags & STATUS_FLAG_PANIC),
            "sensor_error": bool(flags & STATUS_FLAG_ERROR),
        },
    }


class BinaryStreamDecoder:
    def __init__(self, mode: str = "master") -> None:
        if mode not in {"master", "slave", "auto"}:
            raise ValueError(f"Unsupported mode: {mode}")
        self.mode = mode
        self._buf = bytearray()

    def feed(self, data: bytes) -> list[dict]:
        if not data:
            return []
        self._buf.extend(data)
        out: list[dict] = []

        while self._buf:
            b0 = self._buf[0]
            if self.mode in {"master", "auto"} and b0 == GW_SYNC_BYTE:
                if len(self._buf) < GW_STATE_PACKET_SIZE:
                    break
                parsed = unpack_gw_state_packet(bytes(self._buf[:GW_STATE_PACKET_SIZE]))
                if parsed is not None:
                    del self._buf[:GW_STATE_PACKET_SIZE]
                    out.append(format_gw_state_json(parsed))
                    continue
            if self.mode in {"slave", "auto"} and b0 == RS485_SYNC_BYTE:
                if len(self._buf) < RS485_PACKET_SIZE:
                    break
                parsed = unpack_rs485_packet(bytes(self._buf[:RS485_PACKET_SIZE]))
                if parsed is not None:
                    del self._buf[:RS485_PACKET_SIZE]
                    out.append(format_rs485_json(parsed))
                    continue
            del self._buf[:1]

        return out


def _emit(record: dict, compact: bool) -> None:
    if compact:
        print(json.dumps(record, ensure_ascii=False, separators=(",", ":")), flush=True)
    else:
        print(json.dumps(record, ensure_ascii=False, indent=2), flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Lee tramas binarias del Arduino Nano (Maestro o Esclavo) por puerto serie y las formatea en JSON."
    )
    parser.add_argument(
        "-p",
        "--port",
        default="auto",
        help="Puerto serie (ej. /dev/ttyUSB0, /dev/ttyACM0 o 'auto').",
    )
    parser.add_argument(
        "-m",
        "--mode",
        choices=("master", "slave", "auto"),
        default="master",
        help="Protocolo a decodificar: 'master' (gw_state_packet_t, 67B), 'slave' (rs485_packet_t, 8B) o 'auto'.",
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=None,
        help="Velocidad en baudios (por defecto: 115200 en modo master/auto, 9600 en modo slave).",
    )
    parser.add_argument(
        "--poll-id",
        type=int,
        default=None,
        help="En modo slave, envía el byte de sondeo (0xB0 | id) periódicamente (2..5, o 0 para rotar 2..5).",
    )
    parser.add_argument(
        "--poll-interval",
        type=float,
        default=1.0,
        help="Intervalo en segundos entre sondeos cuando se usa --poll-id (por defecto: 1.0).",
    )
    parser.add_argument(
        "-c",
        "--compact",
        action="store_true",
        help="Imprime un objeto JSON por línea (NDJSON) en lugar de formato indentado.",
    )
    parser.add_argument(
        "--stdin",
        action="store_true",
        help="Lee bytes binarios desde stdin en lugar de abrir un puerto serie.",
    )
    args = parser.parse_args()

    decoder = BinaryStreamDecoder(mode=args.mode)

    if args.stdin:
        while True:
            chunk = sys.stdin.buffer.read(64)
            if not chunk:
                break
            for item in decoder.feed(chunk):
                _emit(item, args.compact)
        return

    baudrate = args.baud if args.baud is not None else (9600 if args.mode == "slave" else 115200)
    serial_mod, _ = _load_pyserial()
    last_poll_at = 0.0
    poll_cursor = 2

    try:
        while True:
            target_port = detect_serial_port() if args.port == "auto" else args.port
            if not target_port:
                print("[reader] Esperando puerto serie...", file=sys.stderr, flush=True)
                time.sleep(1.0)
                continue
            try:
                with serial_mod.Serial(target_port, baudrate=baudrate, timeout=0.1) as ser:
                    print(
                        f"[reader] Escuchando en {target_port} @ {baudrate} bps (modo: {args.mode})",
                        file=sys.stderr,
                        flush=True,
                    )
                    while True:
                        now = time.monotonic()
                        if args.poll_id is not None and (now - last_poll_at) >= args.poll_interval:
                            last_poll_at = now
                            if args.poll_id == 0:
                                node_id = poll_cursor
                                poll_cursor = 2 if poll_cursor >= 5 else poll_cursor + 1
                            else:
                                node_id = args.poll_id & 0x0F
                            ser.write(bytes([RS485_POLL_CMD_PREFIX | node_id]))
                            ser.flush()

                        chunk = ser.read(GW_STATE_PACKET_SIZE)
                        if chunk:
                            for item in decoder.feed(chunk):
                                _emit(item, args.compact)
            except Exception as err:
                print(f"[reader] Enlace serie interrumpido ({err}); reintentando...", file=sys.stderr, flush=True)
                time.sleep(1.0)
    except KeyboardInterrupt:
        print("\n[reader] Lectura finalizada.", file=sys.stderr)


if __name__ == "__main__":
    main()
