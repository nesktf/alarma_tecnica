# Guía del proyecto

## Arquitectura (dos capas)

El repositorio es un monorepo con dos capas separadas por una barrera de flasheo:

- `firmware/`: **única capa que se flashea** en la NodeMCU ESP8266. `firmware/server.cpp` inicializa LittleFS/Wi-Fi, mide el sensor ultrasónico, gestiona el bus RS485 (`node_bus.cpp`) y sirve `/api/state` junto con los archivos web; `firmware/sensor.cpp` contiene el muestreo y el formateador del contrato. El build cruzado se controla desde `xmake.lua` en la **raíz** del repositorio (no existe `firmware/xmake.lua`).
- `simulator/`: modelo local con reloj inyectable y servidor HTTP de desarrollo (`127.0.0.1:8080`). **No** es un emulador Xtensa, **no** se flashea y no debe incorporarse al firmware.
- `firmware/static/`: interfaz web limpia de producción (solo lectura, 3 pestañas: Mapa, Grilla y NodeMCU) empaquetada en la imagen LittleFS del ESP8266; sin código ni controles de prueba.
- `static/`: interfaz completa de desarrollo para el simulador con banco de pruebas (`#tabTests`) y controles locales de inyección.
- `tests/`: pruebas nativas del modelo (`test_simulator.py`), de la API HTTP y de contrato (`test_contract.py`).
- `firmware/lib/esp8266/`: core Arduino ESP8266, toolchain y bibliotecas incorporadas; evitar cambios allí salvo que sean necesarios.
- `redes-ii-vault/`: documentación de arquitectura y operación para Obsidian.
- `redes-ii-vault/funcionalidades-pendientes.md`: backlog técnico priorizado y criterios de terminado.

## Comandos habituales

```sh
# Dashboard interactivo en http://127.0.0.1:8080 (Python 3.10+)
python3 -m simulator.server

# Pruebas nativas, sin instalar dependencias (incluyen contrato)
python3 -m unittest discover -s tests -v

# Build real para Xtensa LX106 — desde la RAÍZ del repositorio
# genera build/server.bin y build/littlefs.bin
xmake f -p linux --toolchain=xtensa-lx106-elf
xmake

# Build Arduino Nano (target no predeterminado)
xmake f -p linux --toolchain=avr && xmake build node_station
```

No es necesario conectar una placa para ejecutar el simulador. No ejecutar `xmake flash` como parte de pruebas automatizadas ni afirmar que una prueba local valida hardware real.

> Nota de build: en xmake 3.1.1 la plataforma `cross` no resuelve el toolchain
> local; usar `-p linux --toolchain=xtensa-lx106-elf`. El `xmake.lua` es único y
> vive en la **raíz** del repositorio — invocar `xmake` siempre desde ahí, nunca
> desde `firmware/`. El target predeterminado es `server` (ESP8266). Para compilar
> el Arduino Nano: `xmake f -p linux --toolchain=avr && xmake build node_station`.

## Contrato de estado

La interfaz consulta `GET /api/state`. El firmware y el servidor de simulación deben conservar el mismo contrato JSON de estado, lectura del sensor, conectividad, uptime, pines y perfil de la placa, definido en `shared/api_state.schema.json` y validado por `tests/test_contract.py`.

Los controles `POST /api/controls` existen solo en el simulador; nunca deben aceptar escritura arbitraria desde el firmware. `alarm_system` es parte del contrato base y lo emiten tanto el simulador como el firmware. En firmware sus valores actuales son un estado inicial fijo: no representan una alarma física funcional ni una lectura de sensores PIR. El simulador agrega escenarios de demostración para probar la interfaz; sus umbrales, transiciones y estados no definen la política física. `controls` y `logs` son `preview_only` y solo los emite el simulador.

El simulador escucha solo en `127.0.0.1` por defecto. No exponerlo en una red compartida sin añadir autenticación y controles de acceso.

## Flujo de trabajo: crecer firmware y simulador a la par

Para cada funcionalidad nueva del sistema real, seguir el mismo orden para que el simulador crezca a la par y pueda visualizarla:

1. **Definir el requisito** en `redes-ii-vault/funcionalidades-pendientes.md` antes de codificar (especialmente la política de alarma, hoy pendiente en Prioridad 0).
2. **Extender el contrato** `shared/api_state.schema.json` con las claves nuevas; las de demostración se marcan `preview_only`.
3. **Implementar en el firmware** (`firmware/`), manteniendo buffers pequeños y sin asignación dinámica.
4. **Espejar en el simulador** (`simulator/model.py`) añadiendo el estado y, si aplica, controles de escenario bajo `simulator/scenarios/`.
5. **Probar**: `python3 -m unittest discover -s tests -v` (contrato + modelo + API) y, desde la raíz, `xmake f -p linux --toolchain=xtensa-lx106-elf && xmake` para el build cruzado y su reporte RAM/IRAM/flash.
6. **Documentar**: actualizar `README.md`, `firmware/README.md` y el vault.

Cuando la política de decisión (umbral/histéresis) esté definida, extraerla como módulo C/C++ portable que compile tanto para Xtensa como para tests nativos en host, evitando duplicar la regla en Python.

## Límites físicos

- Objetivo actual: NodeMCU ESP8266, Xtensa LX106 a 80 MHz, 4 MB de flash, variante Arduino `nodemcu`.
- Sensor ultrasónico: TRIG en D6/GPIO12 y ECHO en D5/GPIO14. Confirmar el cableado real antes de alimentar la placa.
- La señal Echo de un sensor alimentado a 5 V necesita adaptación a 3,3 V antes de conectarse al ESP8266.
- Mantener pequeños los buffers, estructuras y dependencias del firmware. Verificar siempre el build cruzado y sus reportes RAM/IRAM/flash; el build nativo no mide los costes del ESP8266.
- No inventar comportamiento de alarma: el umbral y la semántica de “movimiento detectado” siguen pendientes de definición.
- Revisar el backlog del vault antes de añadir funcionalidad de alarma o simulación para no confundir medición de distancia con detección de movimiento.

## Configuración y seguridad

- Las credenciales se generan desde `.env` al compilar; nunca copiar secretos al vault, tests ni salidas de diagnóstico compartidas.
- `build/generated/credentials.h` es generado y está ignorado por Git. No tratarlo como fuente mantenida a mano.
- El directorio `redes-ii-vault/.trash/` es contenido del usuario; no limpiarlo ni reorganizarlo incidentalmente.

## Descubrimiento de skills y MCP

Antes de cada tarea:

1. Revisa las skills disponibles y carga las que correspondan al dominio antes de empezar el trabajo. Las sugerencias automáticas del router ayudan a descubrirlas, pero no son exhaustivas.
2. Revisa las herramientas MCP disponibles y usa las especializadas cuando aporten valor; no invoques herramientas irrelevantes ni dupliques trabajo.
3. Si no hay una skill o MCP pertinente, continúa con el flujo habitual del proyecto.

En la respuesta final, nombra brevemente las skills y MCP que hayas usado.

## Skills compartidas por agentes

Copilot CLI, Antigravity CLI y OpenCode descubren skills locales del proyecto en
`.agents/skills/`. Para cambios de firmware o consultas sobre la placa, usa
`.agents/skills/esp8266-project-context/SKILL.md` junto con la skill de referencia
`.agents/skills/esp8266-nodemcu-30pin/SKILL.md`.

Las reglas de arquitectura y build de este archivo prevalecen sobre ejemplos externos.
