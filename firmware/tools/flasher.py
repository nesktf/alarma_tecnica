#!/usr/bin/env python3
"""
Unified flasher and device detector for ESP8266 (NodeMCU) and AVR (Arduino Nano).
"""

import argparse
import os
import pathlib
import subprocess
import sys
import time

# Add bundled pyserial & esptool from esp8266 tools
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
ESP_TOOLS = REPO_ROOT / "firmware" / "lib" / "esp8266" / "tools"

sys.path.insert(0, (ESP_TOOLS / "pyserial").as_posix())
sys.path.insert(0, (ESP_TOOLS / "esptool").as_posix())

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    sys.stderr.write("Error: PySerial could not be loaded from bundled tools.\n")
    sys.exit(1)


def get_candidate_ports():
    """List likely hardware serial ports (ignoring unused PC ttyS*)."""
    candidates = []
    for port in serial.tools.list_ports.comports():
        # Ignore dummy ttyS0..ttyS31 on Linux unless they have description/hwid
        if port.device.startswith("/dev/ttyS") and port.description == "n/a":
            continue
        candidates.append(port)
    return candidates


def reset_avr_via_dtr(port_name):
    """Cleanly pulse DTR to reset AVR into bootloader mode."""
    try:
        ser = serial.Serial(port_name, baudrate=1200)
        ser.dtr = False
        ser.rts = False
        time.sleep(0.05)
        ser.dtr = True
        ser.rts = True
        time.sleep(0.1)
        ser.dtr = False
        ser.rts = False
        time.sleep(0.05)
        ser.close()
        time.sleep(0.15)
    except Exception:
        pass


def probe_stk500(port_name, baudrate=57600):
    """Probe if device is an AVR running STK500/Optiboot bootloader."""
    try:
        ser = serial.Serial(port_name, baudrate=baudrate, timeout=0.25)
        # Pulse DTR to reset Arduino
        ser.dtr = False
        ser.rts = False
        time.sleep(0.05)
        ser.dtr = True
        ser.rts = True
        time.sleep(0.05)
        ser.dtr = False
        ser.rts = False
        time.sleep(0.15)
        ser.reset_input_buffer()

        for _ in range(4):
            ser.write(b"\x30\x20")  # Cmnd_STK_GET_SYNC, Sync_CRC_EOP
            resp = ser.read(2)
            if resp == b"\x14\x10":  # Resp_STK_INSYNC, Resp_STK_OK
                ser.close()
                return True
            time.sleep(0.05)
        ser.close()
    except Exception:
        pass
    return False


def probe_esp8266(port_name, baudrate=115200):
    """Probe if device is an ESP8266 in bootloader mode or responds to ROM sync."""
    try:
        import esptool
        esp = esptool.ESP8266ROM(port_name, baudrate=baudrate)
        esp._port = serial.Serial(port_name)
        esp.connect("default_reset", connect_mode="default_reset")
        chip_name = esp.get_chip_description()
        esp._port.close()
        if "ESP8266" in chip_name or "ESP" in chip_name:
            return True
    except Exception:
        pass
    return False


def detect_device_and_port(given_port=None):
    """Auto-detect connected device type ('esp8266' or 'nano') and port."""
    candidates = get_candidate_ports()

    target_port = given_port
    if not target_port:
        if len(candidates) == 0:
            print("Error: No serial ports detected. Please connect your board or specify --port.", file=sys.stderr)
            sys.exit(1)
        elif len(candidates) == 1:
            target_port = candidates[0].device
            print(f"Auto-selected serial port: {target_port} ({candidates[0].description})")
        else:
            # Check if any port matches known VID/PID or descriptions
            for cand in candidates:
                desc = (cand.description or "") + " " + (cand.hwid or "")
                if "Arduino" in desc or "FT232" in desc or "2341:" in desc or "2a03:" in desc:
                    target_port = cand.device
                    print(f"Auto-selected Arduino port: {target_port} ({cand.description})")
                    break
            if not target_port:
                print("Multiple serial ports detected:", file=sys.stderr)
                for cand in candidates:
                    print(f"  - {cand.device} ({cand.description} / {cand.hwid})", file=sys.stderr)
                print("Please specify a port with --port=<port>", file=sys.stderr)
                sys.exit(1)

    print(f"Probing {target_port} for board type...")

    # 1. Quick check for Arduino VID / FTDI
    for cand in candidates:
        if cand.device == target_port:
            desc = (cand.description or "") + " " + (cand.hwid or "")
            if "Arduino" in desc or "2341:" in desc or "2a03:" in desc:
                print("Detected Arduino Nano via USB identifier.")
                return "nano", target_port, None

    # 2. Check STK500 at 57600 (Classic / Old Bootloader Nano)
    if probe_stk500(target_port, 57600):
        print("Detected Arduino Nano (ATmega328P Old Bootloader, 57600 baud) via STK500 sync.")
        return "nano", target_port, 57600

    # 3. Check STK500 at 115200 (Nano Optiboot / New Bootloader)
    if probe_stk500(target_port, 115200):
        print("Detected Arduino Nano (Optiboot, 115200 baud) via STK500 sync.")
        return "nano", target_port, 115200

    # 4. Check ESP8266
    if probe_esp8266(target_port, 115200):
        print("Detected ESP8266 board via ROM bootloader sync.")
        return "esp8266", target_port, 115200

    print("Could not definitively identify device type via sync. Defaulting to ESP8266.")
    return "esp8266", target_port, 115200


def flash_esp8266(port, baud, esp_bin, fs_bin=None, flash_all=False):
    """Flash ESP8266 firmware and optionally LittleFS."""
    upload_tool = (ESP_TOOLS / "upload.py").as_posix()
    if not os.path.isfile(esp_bin):
        print(f"Error: ESP8266 binary not found at '{esp_bin}'. Run 'xmake' first.", file=sys.stderr)
        sys.exit(1)

    upload_baud = str(baud or 115200)
    args = [
        sys.executable,
        upload_tool,
        "--chip", "esp8266",
        "--baud", upload_baud,
    ]
    if port:
        args.extend(["--port", port])

    args.extend(["write_flash", "0x0", esp_bin])

    if flash_all:
        if not fs_bin or not os.path.isfile(fs_bin):
            print(f"Error: LittleFS binary not found at '{fs_bin}'. Run 'xmake' first.", file=sys.stderr)
            sys.exit(1)
        args.extend(["write_flash", "0x200000", fs_bin])
        print(f"Flashing ESP8266 firmware ({esp_bin} @ 0x0) and LittleFS ({fs_bin} @ 0x200000) on {port or 'auto'} (baud: {upload_baud})...")
    else:
        print(f"Flashing ESP8266 firmware ({esp_bin} @ 0x0) on {port or 'auto'} (baud: {upload_baud})...")

    ret = subprocess.run(args)
    if ret.returncode != 0:
        sys.exit(ret.returncode)


def run_avrdude(port, baud, nano_hex, mcu="atmega328p", programmer="arduino"):
    """Run avrdude command."""
    args = [
        "avrdude",
        "-v",
        "-p", mcu,
        "-c", programmer,
        "-P", port,
        "-b", str(baud),
        "-D",
        f"-Uflash:w:{nano_hex}:i",
    ]
    print(f"\n>> Flashing via avrdude on {port} (MCU: {mcu}, Programmer: {programmer}, Baud: {baud})...")
    return subprocess.run(args)


def flash_nano(port, baud, nano_hex, mcu="atmega328p", programmer="arduino", bootloader="auto"):
    """Flash Arduino Nano firmware via avrdude with fallback between 57600 and 115200 baud."""
    if not os.path.isfile(nano_hex):
        print(f"Error: Nano hex file not found at '{nano_hex}'. Run 'xmake' first.", file=sys.stderr)
        sys.exit(1)

    if not port:
        candidates = get_candidate_ports()
        if len(candidates) == 1:
            port = candidates[0].device
            print(f"Auto-selected serial port: {port}")
        elif len(candidates) > 1:
            print("Multiple serial ports found. Please specify --port=<port>", file=sys.stderr)
            sys.exit(1)
        else:
            print("No serial port found. Please connect the Arduino Nano or specify --port=<port>", file=sys.stderr)
            sys.exit(1)

    # Determine baud rate sequence to try
    baud_candidates = []
    if baud:
        baud_candidates = [baud]
    elif bootloader == "old":
        baud_candidates = [57600]
    elif bootloader in ["new", "optiboot"]:
        baud_candidates = [115200]
    else:  # auto
        # Try 57600 (classic ATmega328P Nano bootloader) first, then 115200 (Optiboot)
        baud_candidates = [57600, 115200]

    try:
        success = False
        for i, try_baud in enumerate(baud_candidates):
            if i > 0:
                print(f"\nAttempt at {baud_candidates[i-1]} baud failed. Retrying at {try_baud} baud...")
                reset_avr_via_dtr(port)
                time.sleep(0.2)

            ret = run_avrdude(port, try_baud, nano_hex, mcu, programmer)
            if ret.returncode == 0:
                success = True
                break

        if not success:
            print("\n" + "="*70, file=sys.stderr)
            print("Flashing failed with attempted baudrates.", file=sys.stderr)
            print("Manual options to try:", file=sys.stderr)
            print(f"  - Old bootloader: xmake flash -t nano -p {port} -b 57600", file=sys.stderr)
            print(f"  - New bootloader: xmake flash -t nano -p {port} -b 115200", file=sys.stderr)
            print(f"  - ATmega168:      xmake flash -t nano -p {port} -m atmega168 -b 19200", file=sys.stderr)
            print("="*70, file=sys.stderr)
            sys.exit(1)

    except FileNotFoundError:
        print(
            "\nError: 'avrdude' not found in system PATH.\n"
            "To flash Arduino Nano boards, please install avrdude:\n"
            "  - Ubuntu/Debian: sudo apt install avrdude\n"
            "  - Arch Linux:    sudo pacman -S avrdude\n"
            "  - macOS:         brew install avrdude\n",
            file=sys.stderr,
        )
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Unified flasher for ESP8266 and Arduino Nano.")
    parser.add_argument("-t", "--target", default="auto", choices=["auto", "esp8266", "server", "nodemcu", "nano", "arduino", "node_station", "avr"], help="Target board/firmware")
    parser.add_argument("-p", "--port", default=None, help="Serial port (e.g. /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=None, help="Upload baudrate")
    parser.add_argument("-B", "--bootloader", default="auto", choices=["auto", "old", "new", "optiboot"], help="Nano bootloader type")
    parser.add_argument("-a", "--all", action="store_true", help="Flash LittleFS as well as firmware (ESP8266 only)")
    parser.add_argument("--mcu", default="atmega328p", help="Microcontroller for Nano (default: atmega328p)")
    parser.add_argument("--programmer", default="arduino", help="avrdude programmer protocol (default: arduino)")
    parser.add_argument("--esp-bin", default=None, help="Path to server.bin")
    parser.add_argument("--fs-bin", default=None, help="Path to littlefs.bin")
    parser.add_argument("--nano-hex", default=None, help="Path to nano.hex")

    args = parser.parse_args()

    build_dir = REPO_ROOT / "build"
    esp_bin = args.esp_bin or (build_dir / "server.bin").as_posix()
    fs_bin = args.fs_bin or (build_dir / "littlefs.bin").as_posix()
    if args.nano_hex:
        nano_hex = args.nano_hex
    elif (build_dir / "node_station.hex").is_file():
        nano_hex = (build_dir / "node_station.hex").as_posix()
    else:
        nano_hex = (build_dir / "nano.hex").as_posix()

    target_type = args.target.lower()
    port = args.port
    baud = args.baud

    if target_type in ["esp8266", "server", "nodemcu"]:
        flash_esp8266(port, baud or 115200, esp_bin, fs_bin, args.all)
    elif target_type in ["nano", "arduino", "node_station", "avr"]:
        flash_nano(port, baud, nano_hex, args.mcu, args.programmer, args.bootloader)
    else:  # auto
        detected_type, detected_port, detected_baud = detect_device_and_port(port)
        if not baud and detected_baud:
            baud = detected_baud
        if detected_type == "nano":
            flash_nano(detected_port, baud, nano_hex, args.mcu, args.programmer, args.bootloader)
        else:
            flash_esp8266(detected_port, baud or 115200, esp_bin, fs_bin, args.all)


if __name__ == "__main__":
    main()
