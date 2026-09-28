set_project("alarma_tecnica")
set_version("1.0.0")
set_policy("check.auto_ignore_flags", false)

set_defaultplat("cross")
set_targetdir("$(builddir)")

-- ----------------------------------------------------------------------------
-- Toolchain: Xtensa LX106 ELF (bundled in lib/esp8266/tools)
-- ----------------------------------------------------------------------------
toolchain("xtensa-lx106-elf")
    set_kind("standalone")
    set_toolset("cc", "xtensa-lx106-elf-gcc")
    set_toolset("cxx", "xtensa-lx106-elf-g++")
    set_toolset("ld", "xtensa-lx106-elf-gcc")
    set_toolset("ar", "xtensa-lx106-elf-ar")
    set_toolset("as", "xtensa-lx106-elf-gcc")
    set_toolset("strip", "xtensa-lx106-elf-strip")
    set_toolset("objcopy", "xtensa-lx106-elf-objcopy")
    set_toolset("size", "xtensa-lx106-elf-size")

    on_load(function (toolchain)
        local bindir = path.join(os.projectdir(), "lib/esp8266/tools/xtensa-lx106-elf/bin")
        toolchain:add("runenvs", "PATH", bindir)
    end)
toolchain_end()

set_toolchains("xtensa-lx106-elf")

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
    set_default(115200)
    set_description("Upload baud rate (e.g. 115200, 460800)")
option_end()

-- ----------------------------------------------------------------------------
-- Common ESP8266 Arduino rules
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
            "@" .. path.join(os.projectdir(), "lib/esp8266/tools/warnings/none-cxxflags"),
            {force = true}
        )

        target:add("cxxflags", "-fno-rtti", {force = true})
        target:add("asflags", "-g", "-x", "assembler-with-cpp", "-mlongcalls", {force = true})

        target:add("includedirs",
            "lib/esp8266/tools/xtensa-lx106-elf/include",
            "lib/esp8266/tools/sdk/include",
            "lib/esp8266/tools/sdk/lwip2/include",
            "lib/esp8266/cores/esp8266",
            "lib/esp8266/variants/nodemcu",
            "lib/esp8266/libraries/ESP8266WebServer/src",
            "lib/esp8266/libraries/ESP8266WebServer/src/detail",
            "lib/esp8266/libraries/ESP8266WiFi/src",
            "lib/esp8266/libraries/ESP8266WiFi/src/include",
            "lib/esp8266/libraries/LittleFS/src",
            "lib/esp8266/libraries/LittleFS/lib/littlefs",
            "src",
            path.join(target:targetdir(), "generated")
        )
    end)
rule_end()

-- ----------------------------------------------------------------------------
-- Targets
-- ----------------------------------------------------------------------------
target("esp8266_core")
    set_kind("static")
    set_targetdir("$(builddir)")
    add_rules("esp8266.config")
    add_files(
        "lib/esp8266/cores/esp8266/**.c",
        "lib/esp8266/cores/esp8266/**.cpp",
        "lib/esp8266/cores/esp8266/**.S"
    )
target_end()

target("esp8266_wifi")
    set_kind("static")
    set_targetdir("$(builddir)")
    add_rules("esp8266.config")
    add_files("lib/esp8266/libraries/ESP8266WiFi/src/**.cpp")
target_end()

target("esp8266_webserver")
    set_kind("static")
    set_targetdir("$(builddir)")
    add_rules("esp8266.config")
    add_files("lib/esp8266/libraries/ESP8266WebServer/src/**.cpp")
target_end()

target("esp8266_littlefs")
    set_kind("static")
    set_targetdir("$(builddir)")
    add_rules("esp8266.config")
    add_files(
        "lib/esp8266/libraries/LittleFS/src/**.c",
        "lib/esp8266/libraries/LittleFS/src/**.cpp"
    )
target_end()

target("server")
    set_kind("binary")
    set_filename("server.elf")
    set_targetdir("$(builddir)")
    add_rules("esp8266.config")

    add_files("src/server.cpp")

    add_deps("esp8266_littlefs", "esp8266_webserver", "esp8266_wifi", "esp8266_core")

    add_linkdirs(
        "lib/esp8266/tools/sdk/lib",
        "lib/esp8266/tools/sdk/lib/NONOSDK22x_190703",
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

    add_ldflags("-Wl,-Map=" .. path.join(os.projectdir(), "build/server.map"), {force = true})

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
        os.mkdir(path.join(os.projectdir(), "build"))
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
            return table.concat(parts, ", ")
        end

        local raw_ip = env_vars.SERVER_IP or os.getenv("SERVER_IP")
        local raw_gw = env_vars.SERVER_GATEWAY or os.getenv("SERVER_GATEWAY")
        local raw_mask = env_vars.SERVER_MASK or os.getenv("SERVER_MASK")

        local ip_formatted = parse_ip(raw_ip, "SERVER_IP") or "192, 168, 0, 53"
        local gw_formatted = parse_ip(raw_gw, "SERVER_GATEWAY") or "192, 168, 0, 1"
        local mask_formatted = parse_ip(raw_mask, "SERVER_MASK") or "255, 255, 255, 0"

        local header_content = string.format([[#ifndef ENV_H
#define ENV_H

#define WIFI_SSID %q
#define WIFI_PSWD %q
#ifndef WIFI_PASS
#define WIFI_PASS WIFI_PSWD
#endif

#ifndef SERVER_IP
#define SERVER_IP IPAddress(192, 168, 0, 53)
#else
#define SERVER_IP IPAddress(%s)
#endif
#ifndef SERVER_GATEWAY
#define SERVER_GATEWAY IPAddress(192, 168, 0, 1)
#else
#define SERVER_GATEWAY IPAddress(%s)
#endif
#ifndef SERVER_MASK
#define SERVER_MASK IPAddress(255, 255, 255, 0)
#else
#define SERVER_MASK IPAddress(%s)
#endif

#endif // ENV_H
]], wifi_ssid, wifi_pass, ip_formatted, gw_formatted, mask_formatted)

        local outdirs = {gendur, path.join(os.projectdir(), "src")}
        for _, dir in ipairs(outdirs) do
            os.mkdir(dir)
            local target_file = path.join(dir, "credentials.h")
            io.writefile(target_file, header_content)
        end

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
        local bindir = path.join(os.projectdir(), "lib/esp8266/tools/xtensa-lx106-elf/bin")
        local gcc = path.join(bindir, "xtensa-lx106-elf-gcc")
        local outdir = target:targetdir()
        os.mkdir(outdir)

        -- Preprocess linker scripts
        local sdk_ld = path.join(os.projectdir(), "lib/esp8266/tools/sdk/ld")
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
        local tools_dir = path.join(os.projectdir(), "lib/esp8266/tools")
        local bin_dir = path.join(tools_dir, "xtensa-lx106-elf/bin")
        local elf_path = target:targetfile()
        local bin_path = path.join(target:targetdir(), "server.bin")
        local eboot_path = path.join(os.projectdir(), "lib/esp8266/bootloaders/eboot/eboot.elf")

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
        local mklittlefs_tool = path.join(tools_dir, "mklittlefs/mklittlefs")
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
-- Tasks: Flash & Monitor
-- ----------------------------------------------------------------------------
task("flash")
    set_menu({
        usage = "xmake flash [options]",
        description = "Flash firmware (and optionally filesystem) to ESP8266 board",
        options = {
            {'p', "port", "kv", nil, "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud", "kv", 115200, "Upload baudrate"},
            {'a', "all",  "k",  false,  "Flash both firmware and LittleFS filesystem"}
        }
    })

    on_run(function ()
        import("core.base.option")
        import("core.project.project")

        -- Ensure target is built
        os.exec("xmake")

        local target = project.target("server")
        local bin_path = path.join(target:targetdir(), "server.bin")
        if not os.isfile(bin_path) then
            bin_path = path.join(os.projectdir(), "build/server.bin")
        end
        local fs_bin_path = path.join(target:targetdir(), "littlefs.bin")
        if not os.isfile(fs_bin_path) then
            fs_bin_path = path.join(os.projectdir(), "build/littlefs.bin")
        end

        local port = option.get("port")
        local baud = tostring(option.get("baud") or 115200)
        local flash_all = option.get("all")

        local upload_tool = path.join(os.projectdir(), "lib/esp8266/tools/upload.py")
        local args = {upload_tool, "--chip", "esp8266"}
        if port and #port > 0 then
            table.insert(args, "--port")
            table.insert(args, port)
        end
        table.insert(args, "--baud")
        table.insert(args, baud)
        table.insert(args, "write_flash")
        table.insert(args, "0x0")
        table.insert(args, bin_path)

        if flash_all then
            if not os.isfile(fs_bin_path) then
                raise("LittleFS image not found: %s", fs_bin_path)
            end
            table.insert(args, "0x200000")
            table.insert(args, fs_bin_path)
            print("Flashing firmware (%s @ 0x0) and LittleFS (%s @ 0x200000)...", bin_path, fs_bin_path)
        else
            print("Flashing firmware %s @ 0x0 (baud: %s)...", bin_path, baud)
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

        -- Ensure target is built
        os.exec("xmake")

        local target = project.target("server")
        local fs_bin_path = path.join(target:targetdir(), "littlefs.bin")
        if not os.isfile(fs_bin_path) then
            fs_bin_path = path.join(os.projectdir(), "build/littlefs.bin")
        end
        if not os.isfile(fs_bin_path) then
            raise("LittleFS image not found: %s", fs_bin_path)
        end
        local port = option.get("port")
        local baud = tostring(option.get("baud") or 115200)

        local upload_tool = path.join(os.projectdir(), "lib/esp8266/tools/upload.py")
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

task("uploadfs")
    set_menu({
        usage = "xmake uploadfs [options]",
        description = "Alias for flash_fs using the current LittleFS partition",
        options = {
            {'p', "port", "kv", nil, "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud", "kv", 115200, "Upload baudrate"}
        }
    })

    on_run(function ()
        import("core.base.option")
        local args = {"flash_fs"}
        local port = option.get("port")
        local baud = option.get("baud")
        if port and #port > 0 then
            table.insert(args, "-p")
            table.insert(args, port)
        end
        if baud then
            table.insert(args, "-b")
            table.insert(args, tostring(baud))
        end
        os.execv("xmake", args)
    end)
task_end()

task("monitor")
    set_menu({
        usage = "xmake monitor [options]",
        description = "Open serial monitor to ESP8266",
        options = {
            {'p', "port", "kv", nil, "Serial port (e.g. /dev/ttyUSB0)"},
            {'b', "baud", "kv", 9600, "Serial baudrate (default 9600 for server.cpp)"}
        }
    })

    on_run(function ()
        import("core.base.option")
        local port = option.get("port") or "/dev/ttyUSB0"
        local baud = tostring(option.get("baud") or 9600)
        local miniterm = path.join(os.projectdir(), "lib/makeEspArduino/tools/miniterm.py")
        if os.isfile(miniterm) then
            os.execv("python3", {miniterm, "--exit-char", "3", "--rts=0", "--dtr=0", port, baud})
        else
            os.execv("python3", {"-m", "serial.tools.miniterm", "--exit-char", "3", "--rts=0", "--dtr=0", port, baud})
        end
    end)
task_end()
