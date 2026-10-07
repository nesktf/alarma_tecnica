---
tags:
  - redes-ii
  - pendientes
  - firmware
---

# Funcionalidades pendientes

Este documento es el backlog técnico del sistema de alarma. Distingue lo que ya funciona de lo que falta definir, implementar o verificar. La simulación local ayuda a desarrollar el dashboard y algunos estados; **no sustituye** el build Xtensa ni las pruebas en el NodeMCU físico.

## Mapa de documentación actual

- `README.md`: vista general del repositorio, comandos rápidos y estructura del proyecto.
- `Documentacion/Redes II - Servidor web - Informe del proyecto.md`: informe base de la materia y trazado funcional.
- `redes-ii-vault/Arquitectura/Simulacion y arquitectura del sistema.md`: detalles operativos de firmware, simulador y validación.
- `AGENTS.md`: guía operativa del proyecto y lineamientos para trabajo y mantenimiento.

## Estado actual

- [x] Firmware ESP8266 sirve archivos estáticos desde LittleFS.
- [x] Muestreo ultrasónico cada segundo con timeout de 30 ms.
- [x] Endpoint `GET /api/state` para compartir la medición y estado de firmware con la interfaz.
- [x] Dashboard compartido de 4 pestañas (Mapa, Grilla/Eventos, Dispositivo NodeMCU, Banco de Pruebas).
- [x] Simulador local con modelo determinista para distancia, timeout Echo, fallos de arranque, bus RS485 y estados de alarma.
- [x] Protocolo binario RS485 maestro/esclavo y framing CRC-16/CCITT-FALSE definidos en `firmware/protocol.h` y validados con pruebas compartidas.
- [x] Interfaz web restaurada con síntesis de audio Web Audio API, desarmado directo en inspector y sectores proyectados ocultos por defecto.
- [x] Accesibilidad (`prefers-reduced-motion`, `role="meter"` con `aria-valuenow`) y selectores de estado normalizados.
- [x] Pruebas nativas del modelo, API HTTP y **contrato** contra `shared/api_state.schema.json` (30/30 tests OK).
- [x] Imágenes `build/server.bin` y `build/littlefs.bin` para flasheo (build cruzado Xtensa desde la raíz).
- [x] Estructura en dos capas: `firmware/` (se flashea) y `simulator/` (previsualización), con el submódulo ESP8266 en `firmware/lib/esp8266`.
- [x] Pines del sensor actualizados a D6/GPIO12 y D5/GPIO14 para la variante NodeMCU.
- [ ] Confirmar en placa física el sensor/cableado y el divisor o adaptador de nivel de Echo.
- [ ] Integrar el polling de recepción RS485 en el bucle principal de `firmware/server.cpp`.
- [ ] Implementar los anillos de log binarios con cifrado AES-128 CTR en LittleFS para el firmware.

## Flujo de trabajo: crecer firmware y simulador a la par

Para cada funcionalidad nueva del sistema real, seguir el mismo orden para que el simulador crezca a la par y pueda visualizarla:

1. **Definir el requisito** en este backlog antes de codificar.
2. **Extender el contrato** `shared/api_state.schema.json`; las extensiones exclusivas del simulador, como `controls` y `logs`, se marcan `preview_only`. `alarm_system` ya es una clave base obligatoria en ambos entornos.
3. **Implementar en el firmware** (`firmware/`), manteniendo buffers pequeños y sin asignación dinámica.
4. **Espejar en el simulador** (`simulator/model.py`) añadiendo el estado y, si aplica, controles de escenario.
5. **Probar**: `python3 -m unittest discover -s tests -v` (contrato + modelo + API) y, desde la raíz, `xmake f -p linux --toolchain=xtensa-lx106-elf && xmake` para el build cruzado y su reporte RAM/IRAM/flash.
6. **Documentar**: actualizar `README.md`, `firmware/README.md` y este vault.

El firmware actualmente emite valores iniciales fijos para `alarm_system`; eso mantiene el contrato, pero no implementa una alarma física. El simulador ofrece escenarios de demostración que no son una política aprobada. Cuando se definan los requisitos de decisión (umbral/histéresis), extraerlos como módulo C/C++ portable que compile tanto para Xtensa como para tests nativos en host, evitando duplicar la regla en Python.

## Prioridad 0 — Definir el comportamiento del sistema

Estos puntos requieren decisiones funcionales o confirmación del hardware antes de implementar comportamiento de alarma. Los estados y controles existentes en el simulador son datos de demostración, no requisitos aprobados.

### Política de detección y alarma

- [ ] **Definir el fenómeno que debe detectar el sistema y qué sensor lo detecta.** El HC-SR04 entrega distancia; por sí solo no determina movimiento. Aclarar si detecta presencia/cambio de distancia o si la alarma física dependerá de sensores PIR y pulsadores de los nodos.
- [ ] **Definir estados y transiciones:** significado observable de desarmado, armado/en demora, alarma, normal, sensor sin lectura, falla técnica y recuperación. Precisar qué transición activa, mantiene, silencia y restablece cada alarma.
- [ ] **Definir el disparo y despeje:** condiciones, prioridades y posibles combinaciones entre movimiento, pulsadores de emergencia y fallas técnicas. Establecer si la alarma queda enclavada y quién/cuál acción puede desarmarla o reconocerla.
- [ ] **Definir umbrales, unidades e inclusividad de límites** solo para las señales que realmente formen parte de la política. No asignar una distancia del HC-SR04 como umbral de movimiento sin justificar y validar esa interpretación.
- [ ] **Definir filtrado e histéresis:** cantidad y cadencia de muestras, ventana temporal, tratamiento de ruido, lecturas fuera de rango y timeout; especificar también el comportamiento al reiniciar y al recuperar el sensor.
- [ ] **Definir el comportamiento ante lecturas inválidas o sensor desconectado:** estado que se publica, antigüedad máxima de una muestra válida y si la falla produce una alarma técnica independiente de la alarma de intrusión.
- [ ] **Definir el armado:** quién puede armar/desarmar, si existe demora de salida/entrada, cómo se señala el tiempo restante y qué ocurre si hay un sector activo, sin respuesta o en falla durante el armado.
- [ ] **Definir la salida física de alarma** (relay, sirena/timbre u otra), sus estados seguros al arranque y ante pérdida de alimentación/control, límites eléctricos y forma de reconocimiento/silencio.

### Arquitectura y comunicación entre placas

- [ ] **Confirmar la arquitectura física objetivo:** qué funciones corresponden al Arduino Nano central, a cada nodo de sector y al NodeMCU ESP8266. El esquema central RS485 + nodos PIR + NodeMCU como gateway es una propuesta documentada, no una implementación confirmada.
- [ ] **Definir cantidad, identificación y configuración de sectores/nodos**, sensores por sector, pulsadores disponibles, roles (central, remoto, gateway) y cómo se incorporan/reemplazan nodos.
- [ ] **Elegir y validar el medio físico y topología de comunicación** entre central y nodos (incluido RS485 si se mantiene la propuesta): cableado, terminación, polarización, distancias, aislamiento, alimentación y condiciones eléctricas.
- [ ] **Especificar el protocolo de mensajes:** velocidad y parámetros seriales, formato/codificación, delimitación, versión, dirección, tipo de mensaje, campos, unidades, checksum, límites de trama y compatibilidad ante versiones distintas. Definir mensajes periódicos frente a eventos y su cadencia.
- [ ] **Definir confiabilidad del bus:** intervalo de heartbeat, timeout de nodo, reintentos, duplicados, orden, recuperación, colisiones/turnaround RS485 y criterio para marcar un sector como “sin respuesta”.
- [ ] **Definir la autoridad de estado:** cuál placa toma la decisión final de alarma, cómo se sincronizan armado/eventos entre central y gateway y qué datos mantiene el NodeMCU cuando pierde enlace con el central.
- [ ] **Definir qué representa `v_bus`:** punto de medición, rango nominal, calibración, precisión, umbrales de alerta/falla, compensación y reacción del sistema. Los valores del simulador no son límites aprobados.
- [ ] **Definir interfaces de teclado y actuadores** antes de cablear: mapa de teclas, acciones, debounce, confirmaciones, cancelación, indicaciones, pines y estados ante fallas.

### Fallos, operación y seguridad física

- [ ] **Definir matriz de fallos y respuesta segura:** pérdida de energía de central/nodo/gateway, caída o cortocircuito del bus, nodo atascado, sensor abierto/cortocircuitado, tensión baja, reinicio y watchdog. Precisar qué sigue funcionando de forma autónoma.
- [ ] **Definir conectividad y degradación del NodeMCU:** comportamiento si Wi-Fi se pierde después del arranque, no hay DHCP/IP disponible, LittleFS falla o el navegador deja de consultar. Separar el funcionamiento local de la alarma de la disponibilidad del dashboard.
- [ ] **Definir acceso y operación:** si la interfaz de solo lectura en LAN requiere autenticación, quién puede controlar la alarma, protección del endpoint/API, almacenamiento de credenciales y gestión de cambios de configuración.
- [ ] **Confirmar los modelos y revisiones de placas, sensores, relay/sirena y alimentación.** Revisar niveles lógicos, corriente, aislamiento, pines de arranque/UART/flash y adaptación de Echo a 3,3 V antes de conectar.
- [ ] **Definir restricciones de instalación y aceptación eléctrica:** fuente, protecciones, puesta a tierra, longitudes de cable y pruebas de campo pertinentes para la instalación real.

### Contrato, interfaz y criterios de aceptación

- [ ] **Definir el estado público de alarma** en `shared/api_state.schema.json`: nombres estables, códigos, causa/origen, sector, marca temporal, antigüedad y diferencia entre estado físico, estado técnico y datos de demostración.
- [ ] **Definir ciclo de vida y registro de eventos:** qué eventos se guardan, orden y marcas de tiempo, capacidad, persistencia tras reinicio, exportación y política ante memoria llena. Distinguir logs volátiles del simulador de eventos reales.
- [ ] **Definir la experiencia del operador:** estados y textos visibles, accesibilidad, prioridades, reconocimiento/silencio, desconexión/antigüedad de telemetría y presentación distinta de simulación versus hardware.
- [ ] **Definir criterios verificables de aceptación por modo:** pruebas unitarias de política, pruebas de protocolo, escenarios de pérdida/recuperación, build y presupuesto de recursos por placa, y checklist de aceptación física.
- [ ] **Aprobar explícitamente los valores por defecto y rangos** antes de incorporarlos al firmware, esquema, documentación de operación o simulador como comportamiento esperado.

## Prioridad 1 — Completar detección y estados

- [ ] Implementar una política de detección pura y pequeña con los parámetros acordados; evitar asignación dinámica y dependencias pesadas en firmware. Los escenarios de alarma/PIR/V_bus del simulador son demostrativos; los valores fijos que emite el firmware todavía no representan detección física.
- [ ] Extender el estado del dispositivo con la clasificación y el motivo (`normal`, `alarm`, `no_reading`, `sensor_error`, según contrato aprobado).
- [ ] Mostrar en el dashboard un estado visual inequívoco y accesible; distinguir alarma física de fallas del sensor/red/archivos.
- [ ] Añadir escenarios al simulador para umbral, histéresis, muestras ruidosas, timeout intermitente y recuperación.
- [ ] Compartir la lógica de decisión entre firmware y pruebas nativas si se extrae una función compatible con Xtensa; no duplicar una “alarma simulada” que pueda diferir de producción.
- [ ] Añadir pruebas unitarias para límites, transiciones, recuperación y rollover temporal.

## Prioridad 1 — Robustecer arranque y conectividad

- [ ] Sustituir espera indefinida de Wi-Fi por política documentada de timeout/reintento/estado degradado.
- [ ] Evaluar el `HALT()` ante error de LittleFS y configuración de IP; conservarlo solo si el requisito de sistema exige detener el dispositivo.
- [ ] Publicar estados de arranque y conectividad reales; hoy `/api/state` representa firmware en ejecución y reporta Wi-Fi como conectado.
- [ ] Verificar la configuración IP estática desde `.env` en la red real y definir una estrategia para conflictos o gateway no disponible.
- [ ] Definir qué muestra el cliente cuando pierde el endpoint, si conserva la última medición y cuánto tiempo se considera obsoleta.

## Prioridad 1 — Probar la placa y el cableado

- [ ] Conectar TRIG a D6/GPIO12 y ECHO a D5/GPIO14 o elegir y documentar un mapeo alternativo verificado.
- [ ] Confirmar con multímetro/analizador que Echo no supera 3,3 V en el ESP8266. No conectar directamente una salida de 5 V.
- [ ] Probar lecturas del sensor en el rango requerido, timeout, objetos fuera de rango y ruido de alimentación.
- [ ] Confirmar que los pines elegidos no interfieren con arranque, UART, LED integrado, flash ni otros periféricos de la revisión exacta de placa.
- [ ] Medir duración real del bucle, timeout de Echo, latencia HTTP y reinicios/watchdog en el dispositivo.

## Prioridad 1 — Capacidad y estabilidad ESP8266

- [ ] Investigar y reducir consumo de IRAM antes de crecer el firmware: build observado usa 60.287 de 65.536 bytes (aprox. 92 %).
- [ ] Definir umbrales máximos de RAM, IRAM, código en flash y LittleFS para CI/build local; fallar el build si se exceden.
- [ ] Repetir el build Xtensa desde la raíz tras cada cambio de firmware; el test nativo no mide RAM/IRAM, coste de Xtensa ni latencia real.
- [ ] Revisar el límite de concurrencia/tamaño del servidor y los buffers de respuesta cuando se amplíe la API.
- [ ] Comprobar rollover de `millis()` usando aritmética unsigned en el firmware.
- [ ] Medir tamaño y reserva de LittleFS frente a los archivos estáticos que se agreguen.

## Prioridad 2 — Evolucionar API y dashboard

- [ ] Versionar/especificar formalmente el JSON de `GET /api/state` y validar que firmware y simulador respeten el mismo esquema.
- [ ] Añadir estado/clasificación de alarma una vez aprobado el comportamiento, y eventualmente estado de sensor y error de arranque.
- [ ] Definir estrategia de actualización (polling, frecuencia, backoff y expiración). El dashboard consulta actualmente cada segundo y cada navegador añade una consulta por segundo.
- [ ] Añadir mensajes de error útiles y recuperación de conexión en el dashboard.
- [ ] Verificar renderizado en el navegador disponible y diseño móvil; el endpoint estático `/favicon.ico` todavía no tiene recurso confirmado.
- [ ] Probar que las rutas estáticas y `/api/state` conviven correctamente en el ESP8266 real y no exceden memoria al enviar JSON.

## Prioridad 2 — Mejorar la fidelidad de simulación

- [ ] Alinear explícitamente las transiciones del modelo local con la política real elegida, especialmente el hecho de que Wi-Fi/LittleFS fallidos hoy detienen o bloquean el firmware.
- [ ] Simular caída y recuperación de Wi-Fi después del arranque, lectura intermitente y sensor desconectado.
- [ ] Añadir una secuencia grabable/reproducible de valores para ejecutar escenarios temporales largos sin manipulación manual.
- [ ] Agregar pruebas de contrato que comparen las claves JSON requeridas entre el modelo y el firmware (por ejemplo, validar fixtures/check de esquema).
- [ ] Considerar prueba de navegador automatizada solo cuando exista una dependencia y runner adecuados; hoy las pruebas cubren el modelo y HTTP, no render/píxeles ni interacciones reales.
- [ ] Mantener la advertencia visible: el modelo no emula Xtensa, Wi-Fi RF, LittleFS interno, HC-SR04, consumo ni timing eléctrico.

## Prioridad 2 — Seguridad y operación

- [ ] Mantener el simulador ligado a `127.0.0.1`; no usar `--host 0.0.0.0` en redes compartidas sin autenticación y controles de acceso.
- [ ] Si se publican controles o datos fuera de localhost, especificar autenticación, autorización, validación, límites de tasa y política CORS/CSRF antes.
- [ ] Revisar exposición del servidor ESP8266: actualmente la UI/API están en la LAN y no hay autenticación documentada.
- [ ] Asegurar que `.env`, `build/generated/credentials.h`, datos Wi-Fi y credenciales nunca se agreguen a Git ni a notas del vault.
- [ ] Definir actualización OTA solo si se necesita; describir autenticación, verificación de imagen, recuperación y particiones antes de implementarla.

## Prioridad 3 — Mantenibilidad y entrega

- [ ] Mantener `AGENTS.md` y [[Arquitectura/Simulacion y arquitectura del sistema]] actualizados al cambiar comandos, rutas, contrato o pinout.
- [ ] Decidir si `Makefile.` y `lib/makeEspArduino/` son artefactos activos; hoy no forman parte de la guía de build Xmake y no deben incorporarse accidentalmente.
- [ ] Revisar e indexar el repositorio en codebase-memory-mcp después de cambios estructurales, si el servidor está disponible.
- [ ] Crear un flujo automatizado que ejecute pruebas nativas, `node --check static/js/main.js`, build Xtensa y límites de memoria sin flashear una placa.
- [ ] Hacer una prueba de aceptación en hardware con checklist de sensor, Wi-Fi, LittleFS, API, dashboard y recuperación de fallos.

## Criterio de terminado de una función

1. El requisito y sus estados de error están definidos.
2. El escenario relevante puede reproducirse en una prueba nativa cuando aplica.
3. Dashboard y contrato HTTP muestran el estado sin confundir simulación con hardware.
4. Pasa el build Xtensa y se mide el margen de RAM/IRAM/flash.
5. Los requisitos eléctricos/temporales se validan finalmente con la placa y se documenta cualquier limitación.

## Pendientes de hardware Nano (Arduino)

- [ ] Verificar toolchain AVR en xmake.lua: asegurar que `avr-gcc` se detecte correctamente y que las flags `-mmcu=atmega328p` se apliquen.
- [ ] Probar compilación de `nano_core` target: `xmake f -p linux --toolchain=avr && xmake nano_core`.
- [ ] Validar que el firmware Arduino Nano pueda compilarse y enlace correctamente con las dependencias de SPI/I2C si se añaden.
- [ ] Documentar en el vault el proceso de flashing para Arduino Nano usando el bootloader adecuado.
