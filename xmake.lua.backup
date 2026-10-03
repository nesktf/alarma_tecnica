set_project("alarma_tecnica")
set_version("1.0.0")

set_defaultplat("cross")
set_targetdir("$(builddir)")

-- ----------------------------------------------------------------------------
-- Toolchains: Xtensa LX106 ELF (ESP8266) & AVR GCC (Arduino Nano)
-- ----------------------------------------------------------------------------
toolchain("xtensa-lx106-elf")
    set_kind("standalone")
    -- Absolute tool paths: lib/esp8266 stays at the repository root while this
    -- build file lives in firmware/. Resolved from this file's directory so the
    -- project can be moved without breaking the cross-compilation setup.
    local _bindir = path.join(os.scriptdir(), "firmware/lib", "esp8266", "tools", "xtensa-lx106-elf", "bin")
    local _abs = function(name)
        local full = path.join(_bindir, name)
        if not os.isfile(full) then
            raise("xtensa toolchain missing: " .. full)
        end
        return full
    end
    set_toolset("cc", _abs("xtensa-lx106-elf-gcc"))
    set_toolset("cxx", _abs("xtensa-lx106-elf-g++"))
    set_toolset("ld", _abs("xtensa-lx106-elf-gcc"))
    set_toolset("ar", _abs("xtensa-lx106-elf-ar"))
    set_toolset("as", _abs("xtensa-lx106-elf-gcc"))
    set_toolset("strip", _abs("xtensa-lx106-elf-strip"))
    set_toolset("objcopy", _abs("xtensa-lx106-elf-objcopy"))
    set_toolset("size", _abs("xtensa-lx106-elf-size"))

    on_load(function (toolchain)
        toolchain:add("runenvs", "PATH", _bindir)
    end)
toolchain_end()

toolchain("avr")
    set_kind("standalone")

    on_load(function (toolchain)
        local bindir = nil
        local env_avr = os.getenv("AVR_GCC") or os.getenv("AVR_PATH")
        if env_avr and #env_avr > 0 then
            if os.isfile(env_avr) then
                bindir = path.directory(env_avr)
            elseif os.isdir(env_avr) then
                bindir = env_avr
            end
        end
        if not bindir then
            -- Check common Arduino toolchain install directories
            local home = os.getenv("HOME") or ""
            local candidates = {
                path.join(home, ".arduino15/packages/arduino/tools/avr-gcc"),
                "/usr/bin",
                "/usr/local/bin",
                "/opt/arduino/hardware/tools/avr/bin"
            }
            for _, base in ipairs(candidates) do
                if os.isdir(base) then
                    local match = os.files(path.join(base, "**/bin/avr-gcc"))
                    if match and #match > 0 then
                        bindir = path.directory(match[1])
                        break
                    elseif os.isfile(path.join(base, "avr-gcc")) then
                        bindir = base
                        break
                    end
                end
            end
        end
        if bindir then
            toolchain:add("runenvs", "PATH", bindir)
        end
    end)

    set_toolset("cc", "avr-gcc")
    set_toolset("cxx", "avr-g++")
    set_toolset("ld", "avr-gcc")
    set_toolset("ar", "avr-gcc-ar", "avr-ar")
    set_toolset("as", "avr-gcc")
    set_toolset("strip", "avr-strip")
    set_toolset("objcopy", "avr-objcopy")
    set_toolset("size", "avr-size")
toolchain_end()

-- ----------------------------------------------------------------------------
-- Options
-- ----------------------------------------------------------------------------
option("board")
    set_default("nodemcu")
    set_description("Target ESP8266 board (e.g. nodemcu, generic)")
option_end()

option("port")
    set_default("")
    set_description("Serial port for flashing/monitoring (e.g. /dev/ttyUSB0)")
option_end()

option("baud")
    set_default("")
    set_description("Upload baud rate (e.g. 115200, 57600)")
option_end()

option("nano_mcu")
    set_default("atmega328p")
    set_description("Microcontroller for Arduino Nano (e.g. atmega328p, atmega168)")
option_end()

-- ----------------------------------------------------------------------------
-- Rules: ESP8266 & Arduino AVR Nano
-- ----------------------------------------------------------------------------
rule("esp8266.config")
    on_load(function (target)
        target:set("languages", "gnu17", "gnuxx17")

        target:add("defines",
            "__ets__",
            "ICACHE_FLASH",
            "_GNU_SOURCE",
            "ESP8266",
            "MMU_IRAM_SIZE=0x8000",
            "MMU_ICACHE_SIZE=0x8000",
            "FP_IN_IROM",
            "NONOSDK22x_190703=1",
            "F_CPU=80000000L",
            "LWIP_OPEN_SRC",
            "TCP_MSS=536",
            "LWIP_FEATURES=1",
            "LWIP_IPV6=0",
            "ARDUINO=10605",
            "ARDUINO_ESP8266_NODEMCU_ESP12",
            "ARDUINO_ARCH_ESP8266",
            'ARDUINO_BOARD="ESP8266_NODEMCU_ESP12"',
            'ARDUINO_BOARD_ID="nodemcu"',
            "FLASHMODE_QIO",
            "FS_LITTLEFS=1",
            'FS_TYPE="littlefs"'
        )

        target:add("cxflags",
            "-Os",
            "-g",
            "-free",
            "-fipa-pta",
            "-Werror=return-type",
            "-mlongcalls",
            "-mtext-section-literals",
            "-falign-functions=4",
            "-ffunction-sections",
            "-fdata-sections",
            "-fno-exceptions",
            "-U__STRICT_ANSI__",
            "@" .. path.join(os.projectdir(), "firmware/lib/esp8266/tools/warnings/none-cxxflags"),
            {force = true}
        )

        target:add("cxxflags", "-fno-rtti", {force = true})
        target:add("asflags", "-g", "-x", "assembler-with-cpp", "-mlongcalls", {force = true})

        target:add("includedirs",
            "firmware/lib/esp8266/tools/xtensa-lx106-elf/include",
            "firmware/lib/esp8266/tools/sdk/include",
            "firmware/lib/esp8266/tools/sdk/lwip2/include",
            "firmware/lib/esp8266/cores/esp8266",
            "firmware/lib/esp8266/variants/nodemcu",
            "firmware/lib/esp8266/libraries/ESP8266WebServer/src",
            "firmware/lib/esp8266/libraries/ESP8266WebServer/src/detail",
            "firmware/lib/esp8266/libraries/ESP8266WiFi/src",
            "firmware/lib/esp8266/libraries/ESP8266WiFi/src/include",
            "firmware/lib/esp8266/libraries/LittleFS/src",
            "firmware/lib/esp8266/libraries/LittleFS/lib/littlefs",
            "firmware",
            path.join(target:targetdir(), "generated")
        )
    end)
rule_end()

rule("avr_nano.config")
    on_load(function (target)
        target:set("languages", "gnu11", "gnuxx11")

        target:add("defines",
            "F_CPU=16000000L",
            "ARDUINO=10819",
            "ARDUINO_AVR_NANO",
            "ARDUINO_ARCH_AVR",
            "__AVR_ATmega328P__"
        )

        target:add("cxflags",
            "-mmcu=atmega328p",
            "-Os",
            "-g",
            "-Wall",
            "-ffunction-sections",
            "-fdata-sections",
            "-flto",
            "-fno-fat-lto-objects",
            {force = true}
        )

        target:add("cxxflags",
            "-fpermissive",
            "-fno-exceptions",
            "-fno-threadsafe-statics",
            "-Wno-error=narrowing",
            {force = true}
        )

        target:add("asflags",
            "-mmcu=atmega328p",
            "-x", "assembler-with-cpp",
            "-flto",
            {force = true}
        )

        target:add("ldflags",
            "-mmcu=atmega328p",
            "-Os",
            "-g",
            "-flto",
            "-fuse-linker-plugin",
            "-Wl,--gc-sections",
            "-lm",
            {force = true}
        )

        target:add("includedirs",
            "firmware/lib/ArduinoCore-avr/cores/arduino",
            "firmware/lib/ArduinoCore-avr/variants/eightanaloginputs",
            "firmware/lib/ArduinoCore-avr/libraries/EEPROM/src",
            "firmware/lib/ArduinoCore-avr/libraries/Wire/src",
            "firmware/lib/ArduinoCore-avr/libraries/SPI/src",
            "firmware/lib/ArduinoCore-avr/libraries/SoftwareSerial/src",
            "firmware/lib/ArduinoRS485/src",
            "firmware"
        )
    end)
rule_end()

-- ----------------------------------------------------------------------------
-- Targets: ESP8266
-- ----------------------------------------------------------------------------
target("esp8266_core")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("xtensa-lx106-elf")
    add_rules("esp8266.config")
    add_files(
        "firmware/lib/esp8266/cores/esp8266/**.c",
        "firmware/lib/esp8266/cores/esp8266/**.cpp",
        "firmware/lib/esp8266/cores/esp8266/**.S"
    )
target_end()

target("esp8266_wifi")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("xtensa-lx106-elf")
    add_rules("esp8266.config")
    add_files("firmware/lib/esp8266/libraries/ESP8266WiFi/src/**.cpp")
target_end()

target("esp8266_webserver")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("xtensa-lx106-elf")
    add_rules("esp8266.config")
    add_files("firmware/lib/esp8266/libraries/ESP8266WebServer/src/**.cpp")
target_end()

target("esp8266_littlefs")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("xtensa-lx106-elf")
    add_rules("esp8266.config")
    add_files(
        "firmware/lib/esp8266/libraries/LittleFS/src/**.c",
        "firmware/lib/esp8266/libraries/LittleFS/src/**.cpp"
    )
target_end()

target("server")
    set_kind("binary")
    set_default(true)
    set_filename("server.elf")
    set_targetdir("$(builddir)")
    set_toolchains("xtensa-lx106-elf")
    add_rules("esp8266.config")

    add_files("firmware/server.cpp")
    add_files("firmware/sensor.cpp")

    add_deps("esp8266_littlefs", "esp8266_webserver", "esp8266_wifi", "esp8266_core")

    add_linkdirs(
        "firmware/lib/esp8266/tools/sdk/lib",
        "firmware/lib/esp8266/tools/sdk/lib/NONOSDK22x_190703",
        "$(builddir)"
    )

    add_ldflags(
        "-nostdlib",
        "-Wl,--no-check-sections",
        "-Wl,-u,app_entry",
        "-Wl,-u,_printf_float",
        "-Wl,-u,_scanf_float",
        "-Wl,-static",
        "-Wl,--gc-sections",
        "-Wl,-wrap,system_restart_local",
        "-Wl,-wrap,spi_flash_read",
        "-Tlocal.eagle.flash.ld",
        {force = true}
    )

    add_ldflags(
        "-Wl,--start-group",
        "-lesp8266_littlefs", "-lesp8266_webserver", "-lesp8266_wifi", "-lesp8266_core",
        "-lhal", "-lphy", "-lpp", "-lnet80211", "-llwip2-536-feat", "-lwpa", "-lcrypto",
        "-lmain", "-lwps", "-lbearssl", "-lespnow", "-lsmartconfig", "-lairkiss", "-lwpa2",
        "-lstdc++", "-lm", "-lc", "-lgcc",
        "-Wl,--end-group",
        {force = true}
    )

    on_load(function (target)
        local outdir = target:targetdir()
        local gendur = path.join(outdir, "generated")
        os.mkdir(outdir)
        os.mkdir(gendur)

        -- 1. Read .env file & generate credentials.h
        local env_file = path.join(os.projectdir(), ".env")
        local env_vars = {}
        if os.isfile(env_file) then
            local content = io.readfile(env_file)
            for line in content:gmatch("[^\r\n]+") do
                line = line:gsub("^%s+", ""):gsub("%s+$", "")
                if #line > 0 and not line:startswith("#") then
                    local k, v = line:match("^([%w_]+)%s*=%s*(.*)$")
                    if k and v then
                        v = v:gsub('^["\']', ''):gsub('["\']$', '')
                        env_vars[k] = v
                    end
                end
            end
        end

        local wifi_ssid = env_vars.WIFI_SSID or os.getenv("WIFI_SSID")
        local wifi_pass = env_vars.WIFI_PASS or env_vars.WIFI_PSWD or os.getenv("WIFI_PASS") or os.getenv("WIFI_PSWD")

        if not wifi_ssid or #wifi_ssid == 0 then
            raise("Error: WIFI_SSID is required but not defined in .env or environment variables!")
        end
        if not wifi_pass or #wifi_pass == 0 then
            raise("Error: WIFI_PASS is required but not defined in .env or environment variables!")
        end

        local function parse_ip(ip_str, var_name)
            if not ip_str or #ip_str == 0 then
                return nil
            end
            local clean = ip_str:gsub('^["\']', ''):gsub('["\']$', ''):gsub("%s+", "")
            local parts = {}
            for part in clean:gmatch("([^.]+)") do
                local n = tonumber(part)
                if not n or n < 0 or n > 255 then
                    raise(string.format("Invalid IP value in %s: '%s' (component '%s' is not in range 0-255)", var_name, ip_str, part))
                end
                table.insert(parts, tostring(n))
            end
            if #parts ~= 4 then
                raise(string.format("Invalid IP format in %s: '%s' (expected 4 octets)", var_name, ip_str))
            end
            return string.format("IPAddress(%s)", table.concat(parts, ", "))
        end

        local raw_ip = env_vars.SERVER_IP or os.getenv("SERVER_IP")
        local raw_gw = env_vars.SERVER_GATEWAY or os.getenv("SERVER_GATEWAY")
        local raw_mask = env_vars.SERVER_MASK or os.getenv("SERVER_MASK")

        local ip_formatted = parse_ip(raw_ip, "SERVER_IP") or "IPAddress(192, 168, 0, 53)"
        local gw_formatted = parse_ip(raw_gw, "SERVER_GATEWAY") or "IPAddress(192, 168, 0, 1)"
        local mask_formatted = parse_ip(raw_mask, "SERVER_MASK") or "IPAddress(255, 255, 255, 0)"

        local raw_ip_str = raw_ip and raw_ip:gsub('^["\']', ''):gsub('["\']$', ''):gsub("%s+", "") or "192.168.0.53"
        local header_content = string.format([[#ifndef ENV_H
#define ENV_H

#define WIFI_SSID %q
#define WIFI_PSWD %q
#ifndef WIFI_PASS
#define WIFI_PASS WIFI_PSWD
#endif
#define SERVER_IP %s
#define SERVER_IP_STR %q
#define SERVER_GATEWAY %s
#define SERVER_MASK %s

#endif // ENV_H
]], wifi_ssid, wifi_pass, ip_formatted, raw_ip_str, gw_formatted, mask_formatted)

        local target_file = path.join(gendur, "credentials.h")
        io.writefile(target_file, header_content)

        -- 2. Generate buildinfo.h & buildinfo.cpp
        local buildinfo_h = path.join(gendur, "buildinfo.h")
        local buildinfo_cpp = path.join(gendur, "buildinfo.cpp")
        if not os.isfile(buildinfo_h) then
            io.writefile(buildinfo_h, [[
#pragma once
typedef struct { const char *date, *time, *src_version, *env_version; } _tBuildInfo;
extern _tBuildInfo _BuildInfo;
]])
        end
        if not os.isfile(buildinfo_cpp) then
            local date_str = os.date("%Y-%m-%d")
            local time_str = os.date("%H:%M:%S")
            io.writefile(buildinfo_cpp, string.format([[
#include "buildinfo.h"
_tBuildInfo _BuildInfo = {"%s", "%s", "1.0.0", "3.1.2"};
]], date_str, time_str))
        end

        target:add("files", buildinfo_cpp)
    end)

    before_build(function (target)
        local bindir = path.join(os.projectdir(), "firmware/lib/esp8266/tools/xtensa-lx106-elf/bin")
        local gcc = path.join(bindir, "xtensa-lx106-elf-gcc")
        local outdir = target:targetdir()
        os.mkdir(outdir)

        -- Preprocess linker scripts
        local sdk_ld = path.join(os.projectdir(), "firmware/lib/esp8266/tools/sdk/ld")
        local local_flash_ld = path.join(outdir, "local.eagle.flash.ld")
        local local_common_ld = path.join(outdir, "local.eagle.app.v6.common.ld")

        os.execv(gcc, {
            "-x", "c",
            "-DFP_IN_IROM", "-CC", "-E", "-P",
            "-DVTABLES_IN_FLASH",
            "-DMMU_IRAM_SIZE=0x8000",
            "-DMMU_ICACHE_SIZE=0x8000",
            path.join(sdk_ld, "eagle.app.v6.common.ld.h"),
            "-o", local_common_ld
        })

        os.execv(gcc, {
            "-x", "c",
            "-CC", "-E", "-P",
            "-DVTABLES_IN_FLASH",
            "-DMMU_IRAM_SIZE=0x8000",
            "-DMMU_ICACHE_SIZE=0x8000",
            path.join(sdk_ld, "eagle.flash.4m2m.ld"),
            "-o", local_flash_ld
        })
    end)

    after_build(function (target)
        local python = "python3"
        local tools_dir = path.join(os.projectdir(), "firmware/lib/esp8266/tools")
        local bin_dir = path.join(tools_dir, "xtensa-lx106-elf/bin")
        local elf_path = target:targetfile()
        local bin_path = path.join(target:targetdir(), "server.bin")
        local eboot_path = path.join(os.projectdir(), "firmware/lib/esp8266/bootloaders/eboot/eboot.elf")

        -- 1. Generate firmware .bin image
        print("Creating binary image: %s", bin_path)
        os.execv(python, {
            path.join(tools_dir, "elf2bin.py"),
            "--eboot", eboot_path,
            "--app", elf_path,
            "--flash_mode", "qio",
            "--flash_freq", "40",
            "--flash_size", "4M",
            "--path", bin_dir,
            "--out", bin_path
        })

        -- 2. Generate LittleFS .bin filesystem image
        local mklittlefs_tool = path.join("firmware/lib/esp8266/tools", "mklittlefs/mklittlefs")
        local fs_bin_path = path.join(target:targetdir(), "littlefs.bin")
        local static_dir = path.join(os.projectdir(), "static")
        print("Creating LittleFS image: %s", fs_bin_path)
        os.execv(mklittlefs_tool, {
            "-c", static_dir,
            "-p", "256",
            "-b", "8192",
            "-s", "2072576",
            fs_bin_path
        })

        -- 3. Report memory consumption
        os.execv(python, {
            "-X", "utf8",
            path.join(tools_dir, "sizes.py"),
            "--elf", elf_path,
            "--path", bin_dir,
            "--mmu", "-DMMU_IRAM_SIZE=0x8000 -DMMU_ICACHE_SIZE=0x8000"
        })
    end)
target_end()

-- ----------------------------------------------------------------------------
-- Targets: Arduino Nano (AVR ATmega328P)
-- ----------------------------------------------------------------------------
target("nano_core")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("avr")
    add_rules("avr_nano.config")
    add_files(
        "firmware/lib/ArduinoCore-avr/cores/arduino/**.c",
        "firmware/lib/ArduinoCore-avr/cores/arduino/**.cpp",
        "firmware/lib/ArduinoCore-avr/cores/arduino/**.S"
    )
target_end()

target("arduino_rs485")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)")
    set_toolchains("avr")
    add_rules("avr_nano.config")
    add_files("firmware/lib/ArduinoRS485/src/**.cpp")
    add_deps("nano_core")
target_end()

target("node_station")
    set_kind("binary")
    set_default(true)
    set_filename("nano.elf")
    set_targetdir("$(builddir)")
    set_toolchains("avr")
    add_rules("avr_nano.config")

    add_files("firmware/node_station.cpp")
    add_deps("nano_core", "arduino_rs485")

    after_build(function (target)
        local outdir = target:targetdir()
        local elf_path = target:targetfile()
        local hex_path = path.join(outdir, "nano.hex")
        local node_station_hex_path = path.join(outdir, "node_station.hex")

        print("Creating hex image: %s", hex_path)
        os.execv("avr-objcopy", {
            "-O", "ihex",
            "-R", ".eeprom",
            elf_path,
            hex_path
        })
        os.cp(hex_path, node_station_hex_path)

        print("Reporting size for %s:", elf_path)
        try {
            function ()
                os.execv("avr-size", {"-C", "--mcu=atmega328p", elf_path})
            end,
            catch {
                function ()
                    try {
                        function ()
                            os.execv("avr-size", {"-A", elf_path})
                        end
                    }
                end
            }
        }
    end)
target_end()

-- ----------------------------------------------------------------------------
-- Tasks: Flash & Monitor
-- ----------------------------------------------------------------------------
task("flash")
    set_menu({
        usage = "xmake flash [options]",
        description = "Flash firmware to ESP8266 or Arduino Nano board",
        options = {
            {'t', "target",     "kv", "auto",       "Target board/firmware ('esp8266', 'nano', or 'auto')"},
            {'p', "port",       "kv", nil,          "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud",       "kv", nil,          "Upload baudrate (e.g. 57600 for old nano, 115200 for optiboot/esp8266)"},
            {'B', "bootloader", "kv", "auto",       "Nano bootloader type ('auto', 'old' for 57600, 'new' for 115200)"},
            {'a', "all",        "k",  false,        "Flash both firmware and LittleFS filesystem (ESP8266 only)"},
            {'m', "mcu",        "kv", "atmega328p", "Microcontroller for Nano (default: atmega328p)"},
            {'P', "programmer", "kv", "arduino",   "Programmer protocol for Nano (default: arduino)"}
        }
    })

    on_run(function ()
        import("core.base.option")
        import("core.project.project")

        local target_name = option.get("target") or "auto"
        local port = option.get("port")
        local baud = option.get("baud")
        local bootloader = option.get("bootloader") or "auto"
        local flash_all = option.get("all")
        local mcu = option.get("mcu") or "atmega328p"
        local programmer = option.get("programmer") or "arduino"

        -- Build appropriate target or all default targets
        if target_name == "esp8266" or target_name == "server" or target_name == "nodemcu" then
            os.exec("xmake build server")
        elseif target_name == "node_station" or target_name == "nano" or target_name == "arduino" or target_name == "avr" then
            os.exec("xmake build node_station")
        else
            -- auto: build both
            os.exec("xmake")
        end

        local flasher_tool = path.join(os.projectdir(), "firmware/tools/flasher.py")
        local args = {flasher_tool, "-t", target_name}

        if port and #port > 0 then
            table.insert(args, "-p")
            table.insert(args, port)
        end
        if baud and #tostring(baud) > 0 then
            table.insert(args, "-b")
            table.insert(args, tostring(baud))
        end
        if bootloader and #bootloader > 0 then
            table.insert(args, "-B")
            table.insert(args, bootloader)
        end
        if flash_all then
            table.insert(args, "-a")
        end
        if mcu and #mcu > 0 then
            table.insert(args, "--mcu")
            table.insert(args, mcu)
        end
        if programmer and #programmer > 0 then
            table.insert(args, "--programmer")
            table.insert(args, programmer)
        end

        os.execv("python3", args)
    end)
task_end()

task("flash_fs")
    set_menu({
        usage = "xmake flash_fs [options]",
        description = "Flash LittleFS filesystem image to ESP8266 board",
        options = {
            {'p', "port", "kv", nil, "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud", "kv", 115200, "Upload baudrate"}
        }
    })

    on_run(function ()
        import("core.base.option")
        import("core.project.project")

        -- Ensure server target is built
        os.exec("xmake build server")

        local target = project.target("server")
        local fs_bin_path = path.join(target:targetdir(), "littlefs.bin")
        if not os.isfile(fs_bin_path) then
            fs_bin_path = path.join(os.projectdir(), "build/littlefs.bin")
        end
        local port = option.get("port")
        local baud = tostring(option.get("baud") or 115200)

        local upload_tool = path.join(os.projectdir(), "firmware/lib/esp8266/tools/upload.py")
        local args = {upload_tool, "--chip", "esp8266"}
        if port and #port > 0 then
            table.insert(args, "--port")
            table.insert(args, port)
        end
        table.insert(args, "--baud")
        table.insert(args, baud)
        table.insert(args, "write_flash")
        table.insert(args, "0x200000")
        table.insert(args, fs_bin_path)

        print("Flashing LittleFS %s to ESP8266 @ 0x200000 (baud: %s)...", fs_bin_path, baud)
        os.execv("python3", args)
    end)
task_end()

task("monitor")
    set_menu({
        usage = "xmake monitor [options]",
        description = "Open serial monitor to connected board",
        options = {
            {'p', "port", "kv", nil, "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud", "kv", 9600, "Serial baudrate (default 9600)"}
        }
    })

    on_run(function ()
        import("core.base.option")
        local port = option.get("port") or "/dev/ttyUSB0"
        local baud = tostring(option.get("baud") or 9600)
        local miniterm = path.join(os.projectdir(), "firmware/lib/makeEspArduino/tools/miniterm.py")
        if os.isfile(miniterm) then
            os.execv("python3", {miniterm, "--exit-char", "3", "--rts=0", "--dtr=0", port, baud})
        else
            os.execv("python3", {"-m", "serial.tools.miniterm", "--exit-char", "3", "--rts=0", "--dtr=0", port, baud})
        end
    end)
task_end()
