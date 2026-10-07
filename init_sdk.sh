#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

echo "[init_sdk] Updating git submodules..."
git submodule update --init --recursive

TOOLS_DIR="${SCRIPT_DIR}/firmware/lib/esp8266/tools"
if [ ! -d "${TOOLS_DIR}" ]; then
    echo "[init_sdk] Error: ESP8266 tools directory not found at '${TOOLS_DIR}'" >&2
    exit 1
fi

echo "[init_sdk] Fetching ESP8266 toolchain dependencies..."
cd "${TOOLS_DIR}"
python3 ./get.py
cd "${SCRIPT_DIR}"

echo "[init_sdk] SDK initialization completed successfully."

