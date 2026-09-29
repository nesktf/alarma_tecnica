# Guía del proyecto

## Arquitectura

- `src/server.cpp`: firmware para NodeMCU ESP8266; inicializa LittleFS/Wi-Fi, mide el sensor ultrasónico y sirve `/api/state` junto con los archivos web.
- `static/`: interfaz compartida entre el ESP8266 y el simulador local. Mantener compatible con el navegador incluido o previsto para el proyecto.
- `simulator/`: modelo local de la placa y servidor HTTP de desarrollo. No es un emulador Xtensa ni debe incorporarse al firmware.
- `tests/`: pruebas deterministas del modelo y de la API del simulador.
- `lib/esp8266/`: core Arduino ESP8266, toolchain y bibliotecas incorporadas; evitar cambios allí salvo que sean necesarios.
- `redes-ii-vault/`: documentación de arquitectura y operación para Obsidian.
- `redes-ii-vault/funcionalidades-pendientes.md`: backlog técnico priorizado y criterios de terminado.

## Comandos habituales

```sh
# Dashboard interactivo en http://127.0.0.1:8080 (Python 3.10+)
python3 -m simulator.server

# Pruebas nativas, sin instalar dependencias
python3 -m unittest discover -s tests -v

# Build real para Xtensa LX106; también genera LittleFS y build/firmware.bin
xmake server
```

No es necesario conectar una placa para ejecutar el simulador. No ejecutar `xmake flash` como parte de pruebas automatizadas ni afirmar que una prueba local valida hardware real.

## Contrato de estado

La interfaz consulta `GET /api/state`. El firmware y el servidor de simulación deben conservar el mismo contrato JSON de estado, lectura del sensor, conectividad, uptime, pines y perfil de la placa. Los controles `POST /api/controls` existen solo en el simulador; nunca deben aceptar escritura arbitraria desde el firmware.

El simulador escucha solo en `127.0.0.1` por defecto. No exponerlo en una red compartida sin añadir autenticación y controles de acceso.

## Límites físicos

- Objetivo actual: NodeMCU ESP8266, Xtensa LX106 a 80 MHz, 4 MB de flash, variante Arduino `nodemcu`.
- Sensor ultrasónico: TRIG en D6/GPIO12 y ECHO en D5/GPIO14. Confirmar el cableado real antes de alimentar la placa.
- La señal Echo de un sensor alimentado a 5 V necesita adaptación a 3,3 V antes de conectarse al ESP8266.
- Mantener pequeños los buffers, estructuras y dependencias del firmware. Verificar siempre el build cruzado y sus reportes RAM/IRAM/flash; el build nativo no mide los costes del ESP8266.
- No inventar comportamiento de alarma: el umbral y la semántica de “movimiento detectado” siguen pendientes de definición.
- Revisar el backlog del vault antes de añadir funcionalidad de alarma o simulación para no confundir medición de distancia con detección de movimiento.

## Configuración y seguridad

- Las credenciales se generan desde `.env` al compilar; nunca copiar secretos al vault, tests ni salidas de diagnóstico compartidas.
- `src/credentials.h` es generado y está ignorado por Git. No tratarlo como fuente mantenida a mano.
- El directorio `redes-ii-vault/.trash/` es contenido del usuario; no limpiarlo ni reorganizarlo incidentalmente.
