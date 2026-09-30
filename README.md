# Alarma Técnica

Proyecto de monitorización y dashboard local para una NodeMCU ESP8266, con simulador de desarrollo en Python y documentación técnica compartida para firmware y pruebas.

## Estado actual del repositorio

La base actual del proyecto está orientada a una solución en dos capas:

- Firmware ESP8266 en `src/server.cpp`, que sirve la UI estática desde LittleFS y expone `GET /api/state`.
- Simulador local en `simulator/`, que reproduce el mismo contrato observable para pruebas, validación HTTP y desarrollo sin hardware.

El contrato de estado mantiene compatibilidad entre placa y simulador mediante el mismo JSON base, con una diferencia clara: los controles `POST /api/controls` solo existen en el simulador para pruebas y no deben exponerse desde el firmware.

## Estructura principal

- `src/` — firmware y lógica de sensor del NodeMCU.
- `simulator/` — modelo de Estado y servidor HTTP local para pruebas de UI y API.
- `static/` — interfaz web compartida entre firmware y simulador.
- `tests/` — pruebas deterministas del modelo del simulador y de la API HTTP.
- `Documentacion/` — informe académico y material de documentación del proyecto.
- `redes-ii-vault/` — documentación de arquitectura y backlog para Obsidian.
- `lib/esp8266/` — toolchain y bibliotecas Arduino del ESP8266; no se modifica salvo que sea estrictamente necesario.

## Comandos útiles

```sh
# Ejecutar la vista local del simulador
python3 -m simulator.server

# Ejecutar la batería de pruebas nativas
python3 -m unittest discover -s tests -v

# Compilar la versión para NodeMCU ESP8266
xmake server
```

## Convenciones de diseño clave

- El simulador escucha por defecto solo en `127.0.0.1`.
- La interfaz y el estado deben mantenerse compatibles entre firmware y simulador.
- El sensor ultrasónico usa la variante NodeMCU; la asignación actual documentada es TRIG D6/GPIO12 y ECHO D5/GPIO14.
- Si se usa un HC-SR04 alimentado a 5 V, la línea Echo debe adaptarse a 3,3 V antes de conectarla al ESP8266.
- La lógica de alarma aún no está definida; la documentación del vault y del backlog diferencian claramente medición de distancia de detección de movimiento.

## Documentación del proyecto

- `Documentacion/Redes II - Servidor web - Informe del proyecto.md`
- `redes-ii-vault/Arquitectura/Simulacion y arquitectura del sistema.md`
- `redes-ii-vault/funcionalidades-pendientes.md`
- `AGENTS.md`

## Nota de operación

El simulador local no reemplaza la compilación real para Xtensa LX106 ni la verificación física en hardware. Sirve como entorno de validación del contrato web y de escenarios de arranque, conectividad y sensores, pero no mide consumo ni timings del microcontrolador real.
