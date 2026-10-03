---
title: Plan de refactorización de xmake.lua
date: 2026-10-03
tags: [xmake, build-system, refactoring, firmware]
---

# Plan de refactorización de xmake.lua

## Objetivo
Mejorar la estructura, legibilidad y mantenibilidad del archivo `xmake.lua` sin cambiar su funcionalidad.

## Estado actual
El archivo `xmake.lua` actual tiene 714 líneas y contiene:
- Configuración del proyecto
- Definición de toolchains (xtensa-lx106-elf para ESP8266, avr para Arduino Nano)
- Opciones de configuración
- Reglas de compilación (esp8266.config, avr_nano.config)
- Targets: esp8266_core, esp8266_wifi, esp8266_webserver, esp8266_littlefs, server
- Targets Arduino Nano: nano_core, arduino_rs485, node_station
- Tasks: flash, flash_fs, monitor

## Cambios planificados

### 1. Reorganización estructural
- Agregar encabezados de sección claros con separadores visuales
- Agrupar secciones lógicas: Configuración del proyecto, Toolchains, Options, Rules, Targets, Tasks
- Separar targets ESP8266 y Arduino Nano en secciones distintas

### 2. Estandarización de comentarios
- Traducir comentarios en español a inglés
- Estandarizar formato de encabezados de sección
- Eliminar comentarios redundantes
- Mejorar explicaciones en secciones críticas (generación de credentials.h, linker scripts, etc.)

### 3. Formateo consistente
- Indentación consistente (4 espacios)
- Espaciado uniforme entre secciones
- Alineación vertical de set_toolset, set_kind, etc.
- Eliminar espacios en blanco al final de líneas

### 4. Extracción de patrones repetidos (opcional)
- Evaluar si el patrón `_abs` en toolchain xtensa puede generalizarse
- Mantener lógica idéntica, solo reorganizar

### 5. Documentación adicional
- Comentario explicando por qué se usan rutas absolutas para xtensa
- Documentar fallback de AVR toolchain
- Documentar propósito de cada target (server = firmware principal, littlefs = imagen FS, tests = validación en host)

## Cambios que NO se harán
- NO cambiar la lógica de compilación
- NO cambiar flags de compilación ni linker
- NO cambiar rutas de archivos
- NO cambiar toolchains ni configuraciones
- NO agregar/quitar targets, rules, options, tasks

## Verificación
Después de los cambios:
1. `cd firmware && xmake f -p linux --toolchain=xtensa-lx106-elf && xmake build server` - build exitoso
2. `python3 -m unittest discover -s tests -v` - 24 tests pasan
3. Verificar que `server.bin`, `server.elf`, `littlefs.bin` se generan correctamente
4. Verificar que `credentials.h` se genera con `SERVER_IP_STR`

## Archivos afectados
- `xmake.lua` - archivo principal a refactorizar
- `xmake.lua.backup` - backup automático previo a cambios

## Pruebas de regresión
- Build completo: `xmake` (debe compilar todos los targets sin errores)
- Build server: `xmake build server` 
- Tests: `python3 -m unittest discover -s tests -v` (24 tests)
- Verificar archivos generados: `server.bin`, `server.elf`, `littlefs.bin`, `credentials.h`

## Rollback
En caso de problemas: `cp xmake.lua.backup xmake.lua`