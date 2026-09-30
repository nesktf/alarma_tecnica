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
- [x] Dashboard compartido entre NodeMCU y el simulador local.
- [x] Simulador local para distancia, timeout Echo, fallo de Wi-Fi, fallo LittleFS y reinicio.
- [x] Pruebas nativas del modelo y API HTTP.
- [x] Un único `build/firmware.bin` que combina aplicación y LittleFS para flasheo.
- [x] Pines del sensor actualizados a D6/GPIO12 y D5/GPIO14 para la variante NodeMCU.
- [ ] Confirmar en placa el sensor/cableado y el divisor o adaptador de nivel de Echo.

## Prioridad 0 — Definir el comportamiento del sistema

- [ ] **Definir el requisito de alarma:** qué evento físico detecta el sensor y qué significan “Normal”, “Movimiento detectado” y “Desconectado”.
- [ ] **Definir el umbral y las unidades** para activar y limpiar alarma. No asumir que una distancia corta equivale a movimiento: un HC-SR04 mide distancia, no movimiento por sí solo.
- [ ] **Definir histéresis/filtrado:** cuántas muestras activan/desactivan, y qué hacer con ruido, lecturas fuera del rango del sensor o timeout.
- [ ] **Definir pérdida de sensor:** retener estado, pasar a “Sin lectura” o disparar una alarma técnica. Evitar convertir el timeout en `0 cm`.
- [ ] **Definir comportamiento degradado:** qué debe servir el firmware si Wi-Fi cae después del arranque, LittleFS falla o el cliente HTTP deja de consultar.
- [ ] **Confirmar hardware exacto** (variante NodeMCU, revisión de placa y sensor). Revalidar asignación de pines y restricciones de arranque antes de cambiar GPIO.

## Prioridad 1 — Completar detección y estados

- [ ] Implementar una política de detección pura y pequeña con los parámetros acordados; evitar asignación dinámica y dependencias pesadas en firmware.
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
- [ ] Repetir `xmake server` tras cada cambio de firmware; el test nativo no mide RAM/IRAM, coste de Xtensa ni latencia real.
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
- [ ] Asegurar que `.env`, `src/credentials.h`, datos Wi-Fi y credenciales nunca se agreguen a Git ni a notas del vault.
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
