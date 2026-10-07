# Auditoría técnica integral

**Fecha:** 2026-10-06
**Alcance:** código propio del firmware ESP8266, nodo Arduino Nano/RS485, simulador,
API HTTP/WebSocket, frontend, contrato JSON, pruebas y configuración principal de build.

## Resumen ejecutivo

El estado auditado no es apto para considerarlo funcional de punta a punta: las
peticiones HTTP del simulador fallan en el handler, el frontend JavaScript no parsea y
el esquema JSON que inicializa las pruebas de contrato es inválido. El build del
firmware ESP8266 sí termina correctamente, pero no valida la sintaxis JavaScript ni el
contrato. También hay problemas funcionales en la recepción del bus, en la selección
del firmware Nano y en la paridad entre el JSON del ESP8266 y el esquema compartido.

Se encontraron **11 correcciones accionables**. Las prioridades propuestas son:

1. **Bloqueantes:** reparar el enrutamiento HTTP, la sintaxis JS y el JSON del esquema.
2. **Alta prioridad:** conectar la recepción RS485, alinear el artefacto Nano y hacer
   cumplir el contrato de estado.
3. **Seguridad antes de exponer servicios:** autenticar y autorizar en el servidor,
   restringir el proxy y renderizar los datos remotos como texto.
4. **Consistencia y estabilidad:** corregir la afirmación de disponibilidad WebSocket y
   hacer reproducibles los escenarios del simulador.

La auditoría es estática y corresponde al checkout actual, que contiene cambios sin
confirmar. No atribuye esos cambios a una persona ni compara con una versión previa.
No se modificó el código de la aplicación.

## Hallazgos

### 1. El handler HTTP del simulador llama a un método inexistente

**Severidad:** alta — defecto funcional confirmado.
**Ubicación:** `simulator/server.py:205` (y las rutas que delegan en el mismo handler).

`do_GET()` llama a `self._request_path()`, pero `SimulatorHandler` no define ese método.
La primera petición GET produce `AttributeError` y el servidor cierra la conexión sin
respuesta. Esto rompe tanto la carga del dashboard como las rutas GET de estado/API.
Los tests HTTP reproducen el cierre de conexión.

**Corrección:** implementar el parseo/normalización de la ruta dentro del handler o
reemplazar la llamada por el helper existente. Añadir pruebas para `/`, `/api/state`,
rutas desconocidas y query strings.

### 2. El JavaScript del dashboard no puede parsearse

**Severidad:** alta — defecto funcional confirmado.
**Ubicación:** `static/js/main.js:671`.

`node --check static/js/main.js` informa `SyntaxError: Unexpected token '-'` en la
propiedad `no-response` de un objeto. Sin comillas ni corchetes, el archivo completo
falla al parsearse y ninguna inicialización del dashboard puede ejecutarse.

**Corrección:** usar una propiedad válida, por ejemplo `"no-response"` o
`noResponse`, y añadir una comprobación de sintaxis JavaScript al flujo de pruebas.

### 3. El esquema JSON compartido contiene datos extra y no es JSON válido

**Severidad:** alta — bloquea pruebas de contrato.
**Ubicación:** `shared/api_state.schema.json:252`.

`json.loads()` falla con `JSONDecodeError: Extra data` en la línea 252. La inicialización
de `ApiContractTests` lee este archivo, por lo que la suite de contrato no puede
comenzar.

**Corrección:** eliminar la estructura duplicada o mal cerrada y conservar un solo
documento JSON válido. Validar su parseo en CI antes de correr las pruebas semánticas.

### 4. La lectura de respuestas del bus RS485 no se ejecuta

**Severidad:** alta — defecto funcional confirmado.
**Ubicación:** `firmware/server.cpp:155-164`; `firmware/node_bus.cpp:123-146`;
`firmware/node_bus.hpp`.

El ciclo principal ejecuta el polling de transmisión, pero la llamada a
`node_bus_check()` está comentada. Esa función contiene la lectura y validación de
respuestas. No tiene llamadores registrados y tampoco está declarada en el header.
Por lo tanto, el ESP8266 no procesa respuestas de los nodos y no puede actualizar
correctamente su estado online a partir de ellas.

**Corrección:** declarar la operación en el header, invocarla en el ciclo de polling y
añadir pruebas del parser para respuesta válida, CRC inválido, dirección inesperada y
timeout.

### 5. El target del Nano construye un blink, no el nodo RS485 que usa el flasher

**Severidad:** alta — artefacto equivocado confirmado.
**Ubicación:** `xmake.lua:546-558`; `firmware/node_station.cpp:1-12`;
`firmware/nano_node.cpp:88-113`; `firmware/tools/flasher.py:293-299`.

El target `node_station` compila `node_station.cpp`, cuyo `loop()` solo alterna el LED.
El flasher selecciona `build/node_station.hex`, pero el firmware con lógica de nodo
RS485 está en `nano_node.cpp` y no forma parte de ese target. Al seleccionar ese HEX se
programa un blink, no un nodo que responda al controlador.

**Corrección:** enlazar `nano_node.cpp` en el target cuyo artefacto selecciona el
flasher y alinear nombre, ruta y selección del HEX. Incorporar una prueba que confirme
que el build seleccionado contiene el firmware de protocolo.

**Validación pendiente:** no se pudo completar el build AVR en este entorno. Xmake
terminó con `cannot get program for cc`; además, no están disponibles `avr-gcc`,
`avr-g++` ni `avr-objcopy`. La inspección del target y del flasher sí confirma la
selección de fuente/artefacto descrita arriba.

### 6. El JSON del firmware no cumple el contrato publicado

**Severidad:** alta — divergencia confirmada.
**Ubicación:** `firmware/sensor.cpp:49-117`; `shared/api_state.schema.json` (definiciones
de `server` y `alarm_system`); `tests/test_contract.py:107-110`.

El firmware emite `server` como string mientras el esquema lo define como objeto.
También faltan campos requeridos de `alarm_system` y sus sectores; además, el firmware
emite `type` en sectores, propiedad que el esquema no permite. La prueba de contrato
comprueba el snapshot del simulador, pero no la salida del formateador del firmware.

**Corrección:** hacer que el serializador del firmware satisfaga el esquema o modificar
con intención el contrato compartido. Añadir una prueba sobre la salida real de
`format_sensor_state()`. Los valores de alarma pueden continuar como placeholders
documentados: esto no requiere inventar una política física de alarma.

### 7. El control de acceso vive en la interfaz y no protege las rutas

**Severidad:** alta si el servicio se expone fuera de localhost; riesgo condicionado al
despliegue.
**Ubicación:** `firmware/server.cpp:23-28, 101-106`; `simulator/server.py:91-103,
204-241, 296-344`; `static/js/main.js:228-285`.

Las funciones de autenticación (`check_auth()` y `_check_auth()`) no son invocadas por
las rutas. El frontend solo oculta elementos y la verificación del cliente acepta
cualquier credencial que obtenga una respuesta exitosa. El endpoint de controles del
simulador queda accesible sin autenticación del lado servidor; la CLI permite cambiar
la interfaz de escucha, aunque por defecto usa `127.0.0.1`.

**Corrección:** aplicar autenticación y autorización en el servidor a las APIs, archivos
protegidos y WebSocket; no tratar el ocultamiento en el navegador como control de
seguridad. Mantener el bind local por defecto y no exponer el simulador sin una capa de
acceso adecuada. Revisar el almacenamiento de contraseñas en `localStorage`.

### 8. El proxy del simulador puede iniciar conexiones a destinos arbitrarios

**Severidad:** media — riesgo de SSRF condicionado a exponer el servidor.
**Ubicación:** `simulator/server.py:155-173, 204-216, 296-344`.

El endpoint de proxy acepta una URL del cliente y la entrega a `urlopen()` con HTTP y
HTTPS, sin allowlist ni bloqueo de direcciones privadas, loopback o redirecciones hacia
esas direcciones. Si el simulador escucha en una interfaz accesible por terceros, el
proceso puede ser inducido a contactar servicios internos.

**Corrección:** eliminar o desactivar el proxy si no es esencial. Si se mantiene,
restringir hosts y esquemas a una allowlist, bloquear direcciones loopback/privadas y
validar cada destino después de resolver redirecciones.

### 9. El frontend inserta como HTML campos procedentes de estados remotos

**Severidad:** media — riesgo XSS condicionado a consumir una fuente no confiable.
**Ubicación:** `static/js/main.js:709-739, 745-756`; entrada del proxy en
`simulator/server.py:155-173`.

Los renderizadores insertan campos como nombre/estado de sectores y eventos mediante
HTML, sin escapar los valores ni validar el payload recibido. Un estado remoto
controlado por un tercero puede inyectar markup/script en el origen del dashboard.

**Corrección:** usar construcción explícita del DOM y `textContent` para datos; validar
la respuesta remota contra el esquema antes de renderizar. Reservar HTML solo para
plantillas constantes y controladas por la aplicación.

### 10. El estado indica que WebSocket está activo aunque no funciona

**Severidad:** media — inconsistencia funcional confirmada.
**Ubicación:** `firmware/server.cpp:16-18, 101-115, 155-164`;
`simulator/server.py:179-216, 243-252`; `static/js/main.js:156-198, 182-192`.

El firmware informa una conexión WebSocket y el puerto 81 aunque su servidor WebSocket
está deshabilitado. En el simulador, el navegador inicia una actualización HTTP mediante
`GET` con `Upgrade`, pero el servidor intenta iniciar el handshake al recibir
`CONNECT`; además, el intercambio no implementa el encuadre de mensajes RFC 6455. La
interfaz mantiene polling HTTP como respaldo, pero el estado anunciado no coincide con
la capacidad real. El cálculo de retraso exponencial para reconexión tampoco se usa:
el intervalo queda fijo en un segundo.

**Corrección:** implementar handshake y frames compatibles en ambos extremos y reflejar
el estado real, o eliminar la promesa WebSocket hasta implementarla. Aplicar el retraso
calculado o quitar el cálculo si se elige un reintento fijo.

### 11. La simulación base no es reproducible

**Severidad:** baja — mejora de estabilidad y testabilidad.
**Ubicación:** `simulator/model.py:92-117`.

Los muestreos cambian distancias/disponibilidad con aleatoriedad no controlada. Por
tanto, el estado varía entre ejecuciones aunque se use el reloj falso de las pruebas.
Esto dificulta reproducir fallos y comparar snapshots.

**Corrección:** hacer determinista el modelo base, o inyectar un generador aleatorio
sembrable para los escenarios. Añadir una prueba que reproduzca el mismo estado con el
mismo reloj y semilla.

## Verificación ejecutada

| Verificación | Resultado |
|---|---|
| `python3 -m unittest discover -s tests -v` | **Falló:** 19 tests; 7 errores. 12 pasaron. El setup de contrato falla por JSON extra en línea 252; seis pruebas HTTP fallan por `AttributeError: 'SimulatorHandler' object has no attribute '_request_path'` (confirmado al reproducir un test aislado). |
| `node --check static/js/main.js` | **Falló:** error de sintaxis en línea 671. |
| `python3 -m py_compile simulator/model.py simulator/server.py tests/test_contract.py tests/test_simulator.py firmware/tools/flasher.py` | **Pasó.** |
| `xmake f -p linux --toolchain=xtensa-lx106-elf && xmake` | **Pasó:** firmware y LittleFS generados. RAM estática 38%; IRAM 91%; flash 28%. El uso de IRAM deja poco margen para añadir código. |
| Build AVR `xmake f -p linux --toolchain=avr && xmake build node_station` | **No verificado:** error de detección de compilador (`cannot get program for cc`); no hay herramientas AVR instaladas. |
| Flash en hardware | **No ejecutado.** |

El build ESP8266 no detecta problemas de JS ni de contrato porque empaqueta los archivos
web como datos. El uso de IRAM al 91% debe vigilarse en cada cambio de firmware.

## Alcance y limitaciones

Se revisaron fuentes propias, tests, contrato y build. Se excluyeron intencionalmente
`firmware/lib/` y `lib/` (dependencias vendorizadas), artefactos generados, binarios y
datos personales del vault de Obsidian. La cobertura del grafo era parcial en segmentos
del esquema, JavaScript y CSS; esos rangos se revisaron directamente. Un resultado limpio
de cobertura no equivale a una prueba de completitud.

No se verificó cableado físico. El pin Echo de un sensor ultrasónico alimentado a 5 V
requiere adaptación a 3,3 V antes de conectarse al ESP8266; el código no permite
determinar si esa protección está instalada. Los escenarios de alarma del simulador son
de demostración y no definen la política de alarma física pendiente.
