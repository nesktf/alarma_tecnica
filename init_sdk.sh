#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

git submodule update --init --recursive
cd "${SCRIPT_DIR}/lib/esp8266/tools"
python3 ./get.py
cd "${SCRIPT_DIR}"
