# Firmware ESP8266 (capa que se flashea)

Este directorio contiene **el único código que se compila y flashea en la
NodeMCU ESP8266**. Todo lo demás del repositorio (`simulator/`, `static/`,
`tests/`) es apoyo al desarrollo y **nunca** se flashea.

## Contenido

- `src/server.cpp`: `setup()`/`loop()`, arranque (LittleFS → Wi-Fi → servidor),
  LED de estado y `GET /api/state`.
- `src/sensor.cpp`, `src/sensor.hpp`: muestreo ultrasónico cada segundo
  (TRIG D6/GPIO12, ECHO D5/GPIO14, timeout 30 ms) y el formateador del
  contrato JSON de estado.
- `xmake.lua`: build cruzado para Xtensa LX106 con el toolchain y SDK de
  `lib/esp8266` (que permanece en la raíz del repositorio).

## Credenciales

Las credenciales se generan desde `../.env` (raíz del repositorio) al compilar:
`build/generated/credentials.h`. El archivo es generado y está ignorado por Git;
**no** editarlo a mano ni copiarlo al vault, tests ni salidas compartidas.

## Comandos

```sh
# Configurar y compilar ( genera build/server.elf, build/server.bin y
# build/littlefs.bin = LittleFS desde ../static )
xmake f -p linux --toolchain=xtensa-lx106-elf
xmake

# Flashear en la placa (requiere conectar la NodeMCU)
xmake flash --port=/dev/ttyUSB0            # solo firmware
xmake flash --port=/dev/ttyUSB0 --all      # firmware + LittleFS

# Monitor serial
xmake monitor --port=/dev/ttyUSB0
```

> `xmake flash` y `xmake monitor` **no** forman parte de las pruebas
> automatizadas: una prueba local no valida hardware real.

## Reporte de recursos (build 1.0.0, NodeMCU, 80 MHz)

El build real reporta uso de RAM/IRAM/flash. Ejemplo de la migración a dos
capas: RAM 30344/80192 (37%), IRAM 60287/65536 (91%), flash 303028/1 MiB
(28%). Verificar siempre estos números al añadir funcionalidad: el build
nativo del simulador **no** mide los costes del ESP8266.

## Contrato de estado

`GET /api/state` responde el JSON definido en `../shared/api_state.schema.json`.
Las claves base (`board`, `state`, `wifi`, `filesystem`, `sensor`, `pins`,
`limits`, `server`, `alarm_system`, `uptime_ms`, `simulator`) son obligatorias en
firmware y simulador. El firmware emite `alarm_system` con el estado actual del
sistema de alarma. Las claves `controls` y `logs` son exclusivas del simulador y
están marcadas `preview_only` en el esquema.
