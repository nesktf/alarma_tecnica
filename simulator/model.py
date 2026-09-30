"""Deterministic, hardware-independent state model for the local simulator."""

from __future__ import annotations

import math
import threading
import time
from collections import deque
from datetime import datetime
from typing import Callable


class ControlError(ValueError):
    """Raised when a simulator control value is invalid."""


class DeviceModel:
    SAMPLE_INTERVAL_MS = 1000
    ECHO_TIMEOUT_US = 30000
    SENSOR_MIN_CM = 2.0
    SENSOR_MAX_CM = 400.0
    ARMING_DURATION_S = 60
    V_BUS_MIN = 0.0
    V_BUS_MAX = 30.0
    V_BUS_WARNING_V = 11.8
    V_BUS_CRITICAL_V = 11.0
    ALARM_STATES = {"DISARMED", "ARMING", "ARMED", "ALARM"}
    SECTOR_DEFINITIONS = (
        (1, "Área Operativa", "Arduino Nano (central)", True),
        (2, "Automotores", "RS485 nodo 0x02", False),
        (3, "Jefatura de Taller", "RS485 nodo 0x03", True),
        (4, "Vicedirección", "RS485 nodo 0x04", True),
        (5, "Informática", "RS485 nodo 0x05", False),
    )

    def __init__(self, clock: Callable[[], float] = time.monotonic) -> None:
        self._clock = clock
        self._started_at = clock()
        self._lock = threading.RLock()
        self._last_sample_at: float | None = self._started_at
        self._wifi_available = True
        self._filesystem_mounts = True
        self._echo_available = True
        self._distance_cm = 48.0
        self._last_distance_cm: float | None = None
        self._logs: deque[str] = deque(maxlen=30)
        self._alarm_events: deque[dict] = deque(maxlen=50)
        self._alarm_state = "DISARMED"
        self._arming_until: float | None = None
        self._v_bus = 12.4
        self._sectors = {
            sector_id: {
                "id": sector_id,
                "name": name,
                "node": node,
                "has_panic": has_panic,
                "motion": False,
                "active": False,
                "no_response": False,
            }
            for sector_id, name, node, has_panic in self.SECTOR_DEFINITIONS
        }
        self._log("Simulator started; NodeMCU boot sequence complete")

    def _log(self, message: str) -> None:
        timestamp = datetime.now().astimezone().strftime("%H:%M:%S")
        self._logs.append(f"{timestamp}  {message}")

    def record_http_request(self, method: str, path: str) -> None:
        with self._lock:
            normalized = path if path else "/"
            self._log(f"HTTP {method.upper()} {normalized}")

    @property
    def _boot_state(self) -> str:
        if not self._filesystem_mounts:
            return "halted_filesystem"
        if not self._wifi_available:
            return "waiting_wifi"
        return "running"

    def _sample_if_due(self, now: float) -> None:
        if self._boot_state != "running":
            return
        elapsed_ms = 0 if self._last_sample_at is None else (now - self._last_sample_at) * 1000
        if self._last_sample_at is None or elapsed_ms >= self.SAMPLE_INTERVAL_MS - 1e-9:
            self._last_sample_at = now
            if self._echo_available:
                self._last_distance_cm = self._distance_cm
                self._log(f"Distance: {self._distance_cm:.1f} cm")
            else:
                self._last_distance_cm = None
                self._log(f"Echo timeout ({self.ECHO_TIMEOUT_US} us)")

    def snapshot(self) -> dict:
        with self._lock:
            self._update_arming(self._clock())
            return self._snapshot()

    def _snapshot(self) -> dict:
        now = self._clock()
        self._sample_if_due(now)
        state = self._boot_state
        age_ms = None
        if state == "running" and self._last_sample_at is not None:
            age_ms = max(0, int((now - self._last_sample_at) * 1000))

        return {
            "simulator": True,
            "board": "NodeMCU ESP8266",
            "state": state,
            "uptime_ms": max(0, int((now - self._started_at) * 1000)),
            "wifi": self._wifi_available,
            "filesystem": self._filesystem_mounts,
            "sensor": {
                "valid": state == "running" and self._last_distance_cm is not None,
                "distance_cm": self._last_distance_cm if state == "running" else None,
                "age_ms": age_ms,
                "echo_timeout_us": self.ECHO_TIMEOUT_US,
            },
            "pins": {"trigger_gpio": 12, "echo_gpio": 14, "trigger_label": "D6", "echo_label": "D5"},
            "limits": {
                "cpu_mhz": 80,
                "flash_bytes": 4 * 1024 * 1024,
                "dram_bytes": 80192,
                "iram_bytes": 65536,
            },
            "server": {
                "status": "running" if state == "running" else "degraded",
                "host": "127.0.0.1",
                "port": 8080,
                "base_url": "http://127.0.0.1:8080",
                "last_request": "/",
                "routes": [
                    "/",
                    "/api/state",
                    "/css/pico.min.css",
                    "/css/style.css",
                    "/js/main.js",
                ],
            },
            "controls": {
                "distance_cm": self._distance_cm,
                "echo_available": self._echo_available,
                "wifi_available": self._wifi_available,
                "filesystem_mounts": self._filesystem_mounts,
                "sensor_min_cm": self.SENSOR_MIN_CM,
                "sensor_max_cm": self.SENSOR_MAX_CM,
            },
            "alarm_system": self._alarm_snapshot(now),
            "logs": list(self._logs),
        }

    def _alarm_snapshot(self, now: float) -> dict:
        sectors = []
        for sector in self._sectors.values():
            if sector["no_response"]:
                status = "no_response"
            elif sector["active"] or (sector["motion"] and self._alarm_state == "ALARM"):
                status = "alarm"
            elif sector["motion"]:
                status = "motion"
            else:
                status = "normal"
            sectors.append({**sector, "status": status, "projected": False})

        remaining = 0.0
        if self._alarm_state == "ARMING" and self._arming_until is not None:
            remaining = max(0.0, self._arming_until - now)

        v_bus_status = (
            "alarm" if self._v_bus < self.V_BUS_CRITICAL_V
            else "warning" if self._v_bus < self.V_BUS_WARNING_V
            else "normal"
        )
        return {
            "state": self._alarm_state,
            "arming_remaining_s": remaining,
            "v_bus": self._v_bus,
            "v_bus_status": v_bus_status,
            "active_areas": [
                sector["id"] for sector in self._sectors.values()
                if sector["motion"] or sector["active"]
            ],
            "no_response_areas": [
                sector["id"] for sector in self._sectors.values()
                if sector["no_response"]
            ],
            "sectors": sectors,
            "recent_events": list(self._alarm_events),
        }

    def _record_alarm_event(
        self,
        severity: str,
        code: str,
        event: str,
        details: str,
        area_id: int | None = None,
    ) -> None:
        sector = self._sectors.get(area_id) if area_id is not None else None
        self._alarm_events.append({
            "ts": datetime.now().astimezone().strftime("%H:%M:%S"),
            "severity": severity,
            "area": area_id,
            "area_name": sector["name"] if sector else "Sistema",
            "code": code,
            "event": event,
            "details": details,
        })

    def _raise_alarm(self, details: str, area_id: int | None = None) -> None:
        if self._alarm_state == "ALARM":
            return
        self._alarm_state = "ALARM"
        self._arming_until = None
        self._record_alarm_event("critical", "ALARM_TRIGGERED", "Alarma general", details, area_id)
        self._log("Alarm state changed to ALARM")

    def _set_alarm_state(self, state: str, now: float) -> None:
        previous_state = self._alarm_state
        self._alarm_state = state
        self._arming_until = now + self.ARMING_DURATION_S if state == "ARMING" else None

        if state == "DISARMED":
            for sector in self._sectors.values():
                sector["active"] = False

        if previous_state != state:
            self._record_alarm_event(
                "warning" if state in {"ARMING", "ALARM"} else "info",
                f"ALARM_STATE_{state}",
                f"Estado central: {state}",
                f"Transición desde {previous_state}.",
            )
            self._log(f"Alarm state changed to {state}")

        if state == "ARMED":
            for sector in self._sectors.values():
                if sector["motion"] and not sector["no_response"]:
                    self._raise_alarm("Movimiento activo al completar el armado.", sector["id"])
                    break

    def _update_arming(self, now: float) -> None:
        if (
            self._alarm_state == "ARMING"
            and self._arming_until is not None
            and now >= self._arming_until
        ):
            self._set_alarm_state("ARMED", now)

    def _reset_all(self, now: float) -> None:
        self._started_at = now
        self._last_sample_at = now
        self._wifi_available = True
        self._filesystem_mounts = True
        self._echo_available = True
        self._distance_cm = 48.0
        self._last_distance_cm = None
        self._alarm_state = "DISARMED"
        self._arming_until = None
        self._v_bus = 12.4
        for sector in self._sectors.values():
            sector["motion"] = False
            sector["active"] = False
            sector["no_response"] = False
        self._alarm_events.clear()
        self._logs.clear()
        self._log("Simulator reset; NodeMCU boot sequence complete")
        self._record_alarm_event("info", "RESET_ALL", "Reinicio total", "Estado inicial restaurado.")

    def apply_controls(self, controls: dict) -> None:
        with self._lock:
            self._apply_controls(controls)

    def _apply_controls(self, controls: dict) -> None:
        if not isinstance(controls, dict):
            raise ControlError("El cuerpo debe ser un objeto JSON.")
        allowed = {
            "distance_cm",
            "echo_available",
            "wifi_available",
            "filesystem_mounts",
            "reboot",
            "alarm_state",
            "v_bus",
            "toggle_motion",
            "trigger_emergency",
            "toggle_no_response",
            "reset_all",
        }
        unknown = set(controls) - allowed
        if unknown:
            raise ControlError(f"Control desconocido: {', '.join(sorted(unknown))}.")

        distance = self._distance_cm
        if "distance_cm" in controls:
            requested_distance = controls["distance_cm"]
            if isinstance(requested_distance, bool) or not isinstance(requested_distance, (int, float)) or not math.isfinite(requested_distance):
                raise ControlError("distance_cm debe ser un número finito.")
            if not self.SENSOR_MIN_CM <= requested_distance <= self.SENSOR_MAX_CM:
                raise ControlError(
                    f"distance_cm debe estar entre {self.SENSOR_MIN_CM:g} y {self.SENSOR_MAX_CM:g} cm."
                )
            distance = float(requested_distance)

        values = {
            "echo_available": self._echo_available,
            "wifi_available": self._wifi_available,
            "filesystem_mounts": self._filesystem_mounts,
        }
        for key in ("echo_available", "wifi_available", "filesystem_mounts"):
            if key in controls:
                value = controls[key]
                if type(value) is not bool:
                    raise ControlError(f"{key} debe ser true o false.")
                values[key] = value

        reboot = controls.get("reboot", False)
        if type(reboot) is not bool:
            raise ControlError("reboot debe ser true o false.")

        alarm_state = None
        if "alarm_state" in controls:
            alarm_state = controls["alarm_state"]
            if not isinstance(alarm_state, str) or alarm_state not in self.ALARM_STATES:
                raise ControlError(f"alarm_state debe ser uno de: {', '.join(sorted(self.ALARM_STATES))}.")

        v_bus = self._v_bus
        if "v_bus" in controls:
            requested_v_bus = controls["v_bus"]
            if (
                isinstance(requested_v_bus, bool)
                or not isinstance(requested_v_bus, (int, float))
                or not math.isfinite(requested_v_bus)
            ):
                raise ControlError("v_bus debe ser un número finito.")
            if not self.V_BUS_MIN <= requested_v_bus <= self.V_BUS_MAX:
                raise ControlError(
                    f"v_bus debe estar entre {self.V_BUS_MIN:g} y {self.V_BUS_MAX:g} V."
                )
            v_bus = float(requested_v_bus)

        sector_actions = {}
        for control, action in (
            ("toggle_motion", "motion"),
            ("trigger_emergency", "emergency"),
            ("toggle_no_response", "no_response"),
        ):
            if control not in controls:
                continue
            sector_id = controls[control]
            if isinstance(sector_id, bool) or not isinstance(sector_id, int) or sector_id not in self._sectors:
                raise ControlError(f"{control} debe identificar un sector activo entre 1 y 5.")
            if action == "emergency" and not self._sectors[sector_id]["has_panic"]:
                raise ControlError(f"El sector {sector_id} no dispone de pulsador de emergencia.")
            if action == "no_response" and sector_id == 1:
                raise ControlError("El sector 1 es local y no usa el bus RS485.")
            sector_actions[action] = sector_id

        reset_all = controls.get("reset_all", False)
        if type(reset_all) is not bool:
            raise ControlError("reset_all debe ser true o false.")
        if reset_all and (len(controls) != 1 or reboot):
            raise ControlError("reset_all no se puede combinar con otros controles.")

        self._distance_cm = distance
        self._echo_available = values["echo_available"]
        self._wifi_available = values["wifi_available"]
        self._filesystem_mounts = values["filesystem_mounts"]
        now = self._clock()

        if reset_all:
            self._reset_all(now)
            return

        if controls.get("reboot") is True:
            self._started_at = now
            self._last_sample_at = now
            self._last_distance_cm = None
            self._log("Simulated reboot requested")

        previous_v_bus_status = (
            "alarm" if self._v_bus < self.V_BUS_CRITICAL_V
            else "warning" if self._v_bus < self.V_BUS_WARNING_V
            else "normal"
        )
        self._v_bus = v_bus
        next_v_bus_status = (
            "alarm" if self._v_bus < self.V_BUS_CRITICAL_V
            else "warning" if self._v_bus < self.V_BUS_WARNING_V
            else "normal"
        )
        if previous_v_bus_status != next_v_bus_status:
            self._record_alarm_event(
                "critical" if next_v_bus_status == "alarm" else "warning" if next_v_bus_status == "warning" else "info",
                f"V_BUS_{next_v_bus_status.upper()}",
                f"Alimentación de bus: {next_v_bus_status}",
                f"Tensión configurada en {self._v_bus:.1f} V.",
            )

        if "alarm_state" in controls:
            self._set_alarm_state(alarm_state, now)

        if "motion" in sector_actions:
            sector_id = sector_actions["motion"]
            sector = self._sectors[sector_id]
            sector["motion"] = not sector["motion"]
            self._record_alarm_event(
                "warning" if sector["motion"] else "info",
                "MOTION_ON" if sector["motion"] else "MOTION_OFF",
                "Movimiento detectado" if sector["motion"] else "Movimiento cancelado",
                "Entrada PIR virtual activada." if sector["motion"] else "Entrada PIR virtual desactivada.",
                sector_id,
            )
            if sector["motion"] and self._alarm_state == "ARMED" and not sector["no_response"]:
                self._raise_alarm("Movimiento detectado con el sistema armado.", sector_id)

        if "emergency" in sector_actions:
            sector_id = sector_actions["emergency"]
            self._sectors[sector_id]["active"] = True
            self._record_alarm_event(
                "critical",
                "EMERGENCY_TRIGGERED",
                "Pulsador de emergencia",
                "Pulsador virtual activado.",
                sector_id,
            )
            self._raise_alarm("Pulsador de emergencia activado.", sector_id)

        if "no_response" in sector_actions:
            sector_id = sector_actions["no_response"]
            sector = self._sectors[sector_id]
            sector["no_response"] = not sector["no_response"]
            self._record_alarm_event(
                "warning" if sector["no_response"] else "info",
                "BUS_LOST" if sector["no_response"] else "BUS_RESTORED",
                "Sector sin respuesta" if sector["no_response"] else "Comunicación restaurada",
                "Estado virtual del bus RS485 actualizado.",
                sector_id,
            )

        self._log(f"Controls updated; boot state: {self._boot_state}")
