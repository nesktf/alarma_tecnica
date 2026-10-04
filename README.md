# Alarma Técnica

Proyecto de monitorización y dashboard local para una NodeMCU ESP8266, con simulador de desarrollo en Python y documentación técnica compartida para firmware y pruebas.

## Estado actual del repositorio

La base actual del proyecto está orientada a una solución en dos capas:

- **Firmware ESP8266** en `firmware/`: es el único código que se compila y flashea. Sirve la UI estática desde LittleFS y expone `GET /api/state`.
- **Simulador local** en `simulator/`: reproduce el mismo contrato observable para pruebas, validación HTTP y desarrollo sin hardware. **No** es un emulador Xtensa y **no** se flashea.

El contrato de estado mantiene compatibilidad entre placa y simulador mediante el mismo JSON base, con una diferencia clara: los controles `POST /api/controls` solo existen en el simulador para pruebas y no deben exponerse desde el firmware.

## Estructura principal

- `firmware/` — **capa 1, software que se flashea.** `server.cpp`, `sensor.cpp`, `node_station.cpp`. Ver `firmware/README.md`.
- `simulator/` — **capa 2, previsualización.** Modelo de estado determinista y servidor HTTP local (`127.0.0.1:8080`) para pruebas de UI y API.
- `shared/` — contrato compartido: `api_state.schema.json` define el JSON base de `GET /api/state`.
- `static/` — interfaz web compartida entre firmware y simulador (se empaqueta en la imagen LittleFS del firmware).
- `tests/` — pruebas nativas del modelo del simulador, de la API HTTP y de contrato contra el esquema.
- `firmware/lib/esp8266/` — toolchain y bibliotecas Arduino del ESP8266 (submódulo); **no** se modifica salvo que sea estrictamente necesario.
- `Documentacion/` — informe académico y material de documentación del proyecto.
- `redes-ii-vault/` — documentación de arquitectura y backlog para Obsidian.

## Comandos útiles

```sh
# Vista previa del simulador en http://127.0.0.1:8080 (Python 3.10+)
python3 -m simulator.server

# Pruebas nativas: modelo, API HTTP y contrato del esquema
python3 -m unittest discover -s tests -v

# Build cruzado real para Xtensa LX106 (genera build/server.bin y build/littlefs.bin)
# El xmake.lua único vive en la raíz; invocar siempre desde ahí.
xmake f -p linux --toolchain=xtensa-lx106-elf
xmake

# Build del firmware Arduino Nano (AVR ATmega328P) — target no predeterminado
xmake f -p linux --toolchain=avr && xmake build node_station
```

## Convenciones de diseño clave

- El simulador escucha por defecto solo en `127.0.0.1`.
- La interfaz y el estado deben mantenerse compatibles entre firmware y simulador; el esquema `shared/api_state.schema.json` es la fuente de verdad y se valida en `tests/test_contract.py`.
- `alarm_system` es una clave base del contrato y la emiten ambos entornos. En firmware contiene actualmente un estado inicial fijo, no una alarma física funcional; los escenarios más completos del simulador son demostrativos y no fijan la política física. `controls` y `logs` son `preview_only` y solo los emite el simulador.
- No ejecutar `xmake flash` como parte de pruebas automatizadas ni afirmar que una prueba local valida hardware real.
- El archivo `xmake.lua` es único y vive en la raíz del repositorio. No existe ni debe existir un `firmware/xmake.lua` separado.
- El target `server` (ESP8266) es el predeterminado (`set_default(true)`). El target `node_station` (AVR) es `set_default(false)` y debe invocarse explícitamente con `xmake build node_station` usando `--toolchain=avr`.
