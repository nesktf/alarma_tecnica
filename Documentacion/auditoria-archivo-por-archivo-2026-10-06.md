# Auditoría archivo por archivo del proyecto

**Fecha:** 2026-10-06<br>
**Estado evaluado:** checkout local al momento de la auditoría, con cambios ya
presentes y sin confirmar. Los hallazgos describen el contenido encontrado; no
atribuyen autoría ni presuponen que esos cambios sean recientes.<br>
**Tipo:** revisión estática y validación local; no se modificó código de la
aplicación ni se flasheó hardware.

## Conclusión ejecutiva

El proyecto **no está listo para considerarse funcional de punta a punta**. Hay
tres fallos que por sí solos bloquean flujos centrales: el JavaScript no parsea,
el esquema compartido no es JSON válido y el handler HTTP llama a un método
inexistente. Si se resuelven esos bloqueos, todavía quedan desacoples de DOM y
API, contrato firmware/simulador, comunicación RS485 y selección del firmware
Nano. La exposición de servicios fuera de localhost requiere además corregir
controles de acceso y el proxy remoto.

El build Xtensa sí termina, pero ocupa **60.287/65.536 bytes de IRAM (91%)**,
por lo que queda poco margen para incorporar funciones al firmware. No se
verificó el build AVR porque faltan las herramientas de compilación y flasheo.

## Alcance y método

Se revisó el estado actual de las fuentes propias de firmware, bus, simulador,
API, interfaz web, pruebas, build y flasher. Se contrastaron los contratos y
las afirmaciones operativas de `README.md`, `AGENTS.md`, `firmware/README.md` y
la documentación técnica relevante. Los rangos del esquema JSON, JavaScript y
CSS con cobertura parcial en el grafo se comprobaron directamente en el
contenido fuente.

**Exclusiones deliberadas:** `.env` y credenciales generadas; binarios y
directorios de build; bibliotecas/submódulos incorporados (`firmware/lib/`,
`lib/`); CSS/JS de terceros minificados; datos de usuario de Obsidian bajo
`.obsidian/` y `.trash/`; y el informe académico extenso
`Documentacion/Redes II - Servidor web - Informe del proyecto.md`. No se
atribuyen fallas a dependencias de terceros. La colección de skills tampoco
forma parte del código de aplicación auditado.

## Hallazgos priorizados

### F-01 — Bloqueante: el JavaScript del dashboard no puede parsearse

**Severidad:** crítica · **Confianza:** alta<br>
**Archivo:** `static/js/main.js:667-673, 716-722`

Las claves `no-response` de dos objetos no están entrecomilladas. `node
--check` se detiene en la primera, línea 671; la segunda también es sintaxis
inválida. El navegador descarta el script completo, así que no se registra el
listener de `DOMContentLoaded`, no se carga el estado y no se renderiza ninguna
vista.

**Corrección requerida:** hacer válidas ambas claves y agregar `node --check
static/js/main.js` como gate de pruebas.

### F-02 — Bloqueante: todas las rutas HTTP del simulador dependen de un método inexistente

**Severidad:** crítica · **Confianza:** alta<br>
**Archivo:** `simulator/server.py:121-137, 204-220`

`_state_payload()`, `do_GET()` y `do_POST()` llaman a
`self._request_path()`, pero `SimulatorHandler` no define ese método ni importa
un helper equivalente. Las peticiones GET y POST cierran la conexión con
`AttributeError` antes de responder; también se rompe la carga inicial del
dashboard. Las seis pruebas HTTP reproducen el cierre de conexión.

**Corrección requerida:** implementar un único helper de normalización de ruta
basado en `urlsplit`, y probar `/`, `/api/state`, `/api/controls`, query strings
y rutas desconocidas.

### F-03 — Bloqueante: `shared/api_state.schema.json` es inválido

**Severidad:** crítica · **Confianza:** alta<br>
**Archivo:** `shared/api_state.schema.json:252-314`

Después de cerrar el documento en la línea 251 queda un fragmento adicional
que empieza en la línea 252. `python3 -m json.tool` falla con `Extra data` y
`ApiContractTests.setUpClass()` no puede cargar el esquema. El fragmento
duplicado también define otra vez `alarm_system`, `ws`, `auth` y `nodes`, con
reglas distintas —por ejemplo, cambia si `nodes` es `preview_only`—, por lo
que no basta con descartar el error sintáctico sin decidir cuál es la definición
canónica.

**Corrección requerida:** consolidar un solo objeto JSON, resolver las
definiciones duplicadas y validar parseo más validación semántica en las
pruebas.

### F-04 — Alta: el JavaScript y el HTML no tienen una estructura DOM compatible

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `static/index.html:66-70, 228-231, 271-275`;
`static/js/main.js:323-324, 372-384, 461-469, 852-860`;
`static/css/style.css:300-333`

El HTML tiene tres `.tab-pane`, pero cero `.tab-btn` y no incluye controles de
navegación, banco de pruebas ni varios IDs consultados por JS (`tabBtnTests`,
`btnSetDisarmed`, `resetAllBtn`, `stateSourceInput`, `controlError`, entre
otros). CSS oculta todas las pestañas salvo la inicial; sin botones, la grilla
y el panel de dispositivo quedan inaccesibles. Además, `setupTestBench()` entra
al bloque cuando `tabTestsButton` es `null` y luego intenta registrar eventos
en botones nulos. `handleStateUpdate()` también desreferencia
`el.tabTestsButton.hidden`. Aun corrigiendo el parseo y el servidor, la
inicialización/renderizado se detendría por estas referencias.

**Corrección requerida:** decidir qué pantallas/controles se mantienen,
sincronizar el HTML con el contrato DOM del JS y validar al iniciar que las
dependencias requeridas existan. Si se retira el banco de pruebas o el selector
de fuente, retirar también su lógica.

### F-05 — Alta: los controles de estado del cliente usan una clave que la API rechaza

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `static/js/main.js:852-858`; `simulator/model.py:306-319, 429-430`

Los botones envían `set_alarm_state`, pero el modelo solo acepta
`alarm_state`. Cuando esos botones existan y el handler HTTP esté reparado,
estas acciones terminarán en `ControlError`/HTTP 400. Los controles
`toggle_motion`, `trigger_emergency` y `toggle_no_response` sí usan los nombres
esperados.

**Corrección requerida:** unificar nombres en UI, API, modelo y pruebas de
contrato.

### F-06 — Alta: la respuesta del firmware contradice el contrato publicado

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `firmware/sensor.cpp:87-115`;
`shared/api_state.schema.json:49-55, 170-215, 217-247`

El firmware emite `server` como string, aunque el esquema requiere un objeto;
`sensor.age_ms` vale `-1` cuando no hay lectura, pese a que el esquema permite
solo entero no negativo o `null`. `alarm_system` omite campos requeridos como
`arming_remaining_s` y campos obligatorios de cada sector, y agrega propiedades
que la definición con `additionalProperties: false` no admite (`state_code`,
`motion_areas`, `type`). La respuesta también afirma `wifi:true` y estados
`running` sin comprobar el enlace después del arranque. `tests/test_contract.py`
solo valida el snapshot del simulador, no el formateador real del firmware.

**Corrección requerida:** acordar primero el contrato único; luego serializar
todos los campos base de forma consistente y agregar una prueba de la salida
real de `format_sensor_state()` en host. Mantener explícito que los valores de
alarma son placeholders mientras la política física siga pendiente.

### F-07 — Alta: el firmware nunca procesa respuestas RS485

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `firmware/server.cpp:155-163`; `firmware/node_bus.cpp:123-146`;
`firmware/node_bus.hpp:41-57`

`loop()` invoca `node_bus_poll()` pero deja comentado `node_bus_check()`, que
es donde se leen y validan las tramas. `node_bus_check()` tampoco está
declarada en el header. Así no se actualizan `online`, distancia, estado ni
`last_seen_ms` desde respuestas reales.

**Corrección requerida:** completar y exponer la API de recepción, integrar un
parser incremental no bloqueante al loop y añadir pruebas de trama válida,
CRC inválido, ruido, nodo inesperado y timeout. No considerar operativo el bus
hasta validar la topología y el hardware RS485 reales.

### F-08 — Alta: el polling RS485 no puede atender varios nodos y su ventana temporal es insuficiente

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `firmware/node_bus.cpp:70-105`; `firmware/node_bus.hpp:22-28`;
`firmware/nano_node.cpp:18, 58-60, 94-110`

El selector no mantiene un cursor de rotación: sin nodos online siempre vuelve
a consultar el primero; si el nodo 1 queda online, lo vuelve a elegir en cada
ciclo. `nano_node.cpp` responde únicamente a `NANO_NODE_ID == 1`, así que no
hay una configuración que permita desplegar varios IDs. Además, el maestro
vence a los 20 ms, pero el esclavo puede tardar hasta 30 ms en `pulseIn()` y
luego transmitir siete bytes a 9600 baud. El maestro baja DE inmediatamente
después de `Serial.write()`, sin esperar a que el byte termine de salir, con
riesgo de truncar el comando.

**Corrección requerida:** definir el protocolo, direccionamiento, turnaround,
timeout total y política de reintento según el hardware; implementar un
scheduler circular; esperar fin de transmisión antes de cambiar DE; y probar
la secuencia con nodos simulados y luego en el bus físico.

### F-09 — Alta: el target Nano que se flashea contiene un blink, no el nodo RS485

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `xmake.lua:543-568`; `firmware/node_station.cpp:1-12`;
`firmware/nano_node.cpp:1-113`; `firmware/tools/flasher.py:291-305`

El target `node_station` compila solo `node_station.cpp`, que alterna el LED.
`nano_node.cpp`, que contiene la lógica de sensor/protocolo, no se compila en
ningún target. Xmake genera `node_station.hex` a partir del mismo ELF blink y el
flasher elige ese HEX para el Nano. Por tanto, el artefacto seleccionado no
responde al protocolo RS485.

**Corrección requerida:** decidir si el blink es solo un diagnóstico separado;
construir el firmware de nodo como target explícito, usar el artefacto correcto
en el flasher y probar que source/target/HEX coincidan. No se verificó
compilación AVR en este entorno.

### F-10 — Alta condicionada a exposición: la autenticación es solo aparente

**Severidad:** alta si el servicio se expone a una red · **Confianza:** alta<br>
**Archivos:** `firmware/server.cpp:23-28, 101-105`;
`simulator/server.py:23-24, 91-110, 204-224, 296-332`;
`static/js/main.js:250-279, 446-454, 493-516`

El servidor Python define `_check_auth()` pero no la llama en ninguna ruta; el
ESP8266 define `check_auth()` y tampoco la aplica al contenido o a `/api/state`.
Ambos anuncian `authenticated: true` aunque no se ha autenticado la petición.
El frontend almacena usuario y contraseña en `localStorage`; solo manda una
cabecera Basic durante la comprobación inicial, no en el polling ni en los
controles. Las contraseñas `admin/admin` están compiladas/configuradas como
constantes por defecto. El simulador permite bind fuera de localhost y el
firmware sirve por Wi-Fi.

**Corrección requerida:** elegir y documentar el modelo de acceso antes de
exponer los servicios; aplicar autorización en servidor a cada ruta, no solo
ocultar la UI; eliminar credenciales por defecto y no almacenar contraseñas en
`localStorage`. Basic Auth sin TLS no protege credenciales frente a escucha en
la red.

### F-11 — Media/alta condicionada a exposición: el proxy permite SSRF y respuestas sin límite

**Severidad:** media en localhost; alta si el simulador acepta clientes de red ·
**Confianza:** alta<br>
**Archivos:** `simulator/server.py:155-173, 204-212, 296-307`;
`tests/test_simulator.py:243-268`

`/api/proxy` acepta cualquier URL HTTP/HTTPS y la consulta con `urlopen()`.
No restringe loopback, direcciones privadas, redirecciones ni destinos
internos; además usa `response.read()` sin límite de tamaño. Si el servidor se
expone, un cliente puede inducir solicitudes desde el equipo del simulador a
servicios internos y consumir memoria con una respuesta grande. La prueba
actual confirma que el proxy puede consultar `127.0.0.1`.

**Corrección requerida:** quitar/desactivar el proxy si no es necesario; si se
conserva, usar allowlist de hosts/esquemas, bloquear direcciones privadas y
redirecciones hacia ellas, limitar bytes/tiempo y validar el estado recibido.

### F-12 — Media, latente: renderizado HTML de datos del estado sin escape

**Severidad:** media condicional · **Confianza:** media-alta<br>
**Archivo:** `static/js/main.js:709-755`

`sec.name`, `sec.status` y los campos de eventos se interpolan en
`innerHTML`. El endpoint proxy puede entregar JSON externo. En el HTML actual
no existe `stateSourceInput`, por lo que el flujo proxy de la UI no queda
habilitado desde la página tal como está; aun así, al conectar ese flujo, una
respuesta externa podría insertar markup en el origen del dashboard.

**Corrección requerida:** construir nodos y asignar datos con `textContent`;
validar la estructura de estado antes del render. Tratar el hallazgo como
condicional hasta que la fuente remota quede conectada.

### F-13 — Alta: WebSocket anunciado, pero no implementado de extremo a extremo

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `firmware/server.cpp:19-20, 34-55, 108-115, 157-159`;
`simulator/server.py:179-202, 218-230, 330-344`;
`static/js/main.js:156-202`; `shared/api_state.schema.json:217-224`

El firmware deshabilita WebSocket, pero el JSON anuncia conexión y puerto 81.
El simulador espera `CONNECT`, mientras el navegador negocia WebSocket mediante
`GET` con `Upgrade`. Incluso si se alcanzara `_upgrade_websocket`, transmite
JSON crudo sin frames RFC 6455; tampoco interpreta frames enmascarados ni el
ping del cliente. `connected` se fija en `true` aunque no haya clientes; el
`--ws-port` configurado no se refleja en el JSON/cliente. El cliente calcula
un backoff pero siempre espera un segundo y no evita temporizadores de
reconexión duplicados.

**Corrección requerida:** o bien implementar un WebSocket estándar y reportar
conectividad/configuración reales, o retirar temporalmente el canal del
contrato y depender del polling HTTP.

### F-14 — Media: el buffer de nodos puede omitir nodos sin reportar truncamiento

**Severidad:** media · **Confianza:** alta<br>
**Archivo:** `firmware/sensor.cpp:57-82`

La lista completa de hasta nueve nodos se arma en 512 bytes. Con los campos
actuales, nueve entradas pueden superar ese límite; el código omite las
entradas que ya no caben sin marcar error ni indicar truncamiento, y luego
responde HTTP 200 con una lista incompleta. Esto impide confiar en el estado
publicado incluso cuando la trama RS485 funcione.

**Corrección requerida:** calcular y documentar el límite máximo, aumentar o
eliminar el buffer intermedio con un writer acotado, y devolver error
explícito si no cabe el documento completo.

### F-15 — Media: el modelo del simulador no es determinista y el reset no restablece nodos

**Severidad:** media · **Confianza:** alta<br>
**Archivo:** `simulator/model.py:1, 51-59, 92-117, 279-297`

El docstring lo llama determinista, pero cada muestreo usa `random.randint()` y
`random.random()` globales para variar distancias y disponibilidad. El reloj
inyectado no controla esa aleatoriedad; por eso snapshots y pruebas pueden
variar entre ejecuciones. `_reset_all()` restaura el sensor y la alarma, pero
no reconstruye `_nodes_state`, así que la distancia/estado de nodos conserva
el drift previo a un “reset total”.

**Corrección requerida:** hacer determinista el escenario base o inyectar un
generador sembrable; definir y probar qué estado restablece `reset_all`.

### F-16 — Media: el estado RS485 del simulador es sintético y contradictorio

**Severidad:** media · **Confianza:** alta<br>
**Archivos:** `simulator/model.py:51-59, 132-178`;
`simulator/server.py:147-152`

`DeviceModel.snapshot()` crea nodos online con `status: 1`, mientras el bit 0
del protocolo significa alarma; el broadcaster WebSocket publica esa versión.
En HTTP, `_state_payload()` reemplaza todos los nodos por otros valores
fabricados (`status: 4`, online, distancia 0, `last_seen_ms: 0`). Los controles
de `no_response` solo cambian sectores de alarma y no se reflejan en
`nodes`. Por tanto, HTTP y WS no describen la misma fuente ni el estado real
del escenario.

**Corrección requerida:** mantener una sola representación de nodos en el
modelo; derivar los estados de ambas rutas desde ella y marcar explícitamente
como demostración los datos simulados.

### F-17 — Media: la cobertura de pruebas no alcanza los contratos críticos

**Severidad:** media · **Confianza:** alta<br>
**Archivos:** `tests/test_contract.py:27-95, 98-110`;
`tests/test_simulator.py:175-268`

El validador casero implementa solo un subconjunto de JSON Schema Draft 7 y
no valida el formateador C++ del firmware. El setup falla actualmente antes
de validar snapshots por el JSON roto. No hay pruebas para parser/CRC RS485,
turnaround, reparto entre IDs, WebSocket real, autenticación, límite de bytes
del proxy o XSS. Las pruebas HTTP existentes descubren el helper faltante,
pero las fallas del setup impiden una suite verde.

**Corrección requerida:** primero restaurar una suite ejecutable; después usar
un validador estándar disponible o limitar/documentar el subconjunto; agregar
pruebas del serializador de firmware, protocolo de bus y flujos HTTP/JS
observables. Mantener pruebas de hardware separadas de las nativas.

### F-18 — Alta: el modo de flasheo automático puede elegir el firmware incorrecto o una imagen obsoleta

**Severidad:** alta · **Confianza:** alta<br>
**Archivos:** `firmware/tools/flasher.py:87-100, 103-110, 306-313`;
`xmake.lua:603-635`

Si no reconoce el dispositivo, `detect_device_and_port()` retorna ESP8266 por
defecto. En `auto`, eso puede enviar la imagen ESP al dispositivo conectado
aunque no se haya identificado con certeza. El comando Xmake `flash -t auto`
construye solo el target predeterminado antes de detectar la placa; no construye
necesariamente el target Nano y podría reutilizar un HEX previo. Además,
`probe_esp8266()` usa el atributo privado `esp._port` y las rutinas de probe
suprimen excepciones; si fallan antes de cerrar el puerto, no existe un
`finally` que garantice su liberación.

**Corrección requerida:** no asumir ESP ante detección ambigua; detenerse y
pedir selección explícita. Determinar el dispositivo antes de construir su
target, verificar fecha/origen del artefacto y cerrar puertos con contextos o
`finally`.

### F-19 — Media: opciones AVR declaradas no controlan realmente el target y el margen IRAM es pequeño

**Severidad:** media · **Confianza:** alta<br>
**Archivo:** `xmake.lua:41-77, 185-229, 342-373, 543-568`

La opción `nano_mcu` se declara, pero las flags y define fijan
`atmega328p`; cambiar `--mcu` en el flasher no cambia cómo se compiló el
firmware. La búsqueda AVR ordena rutas como texto descendente, no por versión
semántica. El build Xtensa actual finaliza con 91% de IRAM usada y no existe un
límite automatizado que avise/falle al cruzar un umbral. El mensaje de error
indica copiar `.env.example`, pero no hay `.env.example` en el repositorio.

**Corrección requerida:** o quitar opciones no soportadas o hacer que
configuren compilación y flasheo conjuntamente; ordenar versiones de forma
semántica; añadir guardas de recursos; y aportar instrucciones/plantilla de
configuración sin secretos.

### F-20 — Baja/media: la decodificación del CRC depende de alineación y endianness implícitos

**Severidad:** baja-media · **Confianza:** alta<br>
**Archivo:** `firmware/node_bus.cpp:26-49`

La trama se compara leyendo `uint16_t` desde `raw + 5`. Esa dirección no
garantiza alineación para `uint16_t` y depende del endianness nativo, aunque el
protocolo ya define el CRC en little endian. Puede compilar y funcionar en los
targets actuales, pero es una lectura no portable y propensa a comportamiento
indefinido bajo las reglas de C++.

**Corrección requerida:** decodificar CRC combinando explícitamente los dos
bytes y añadir un test vector compartido Nano/ESP.

### F-21 — Baja/media: el script de preparación puede terminar con éxito después de fallar

**Severidad:** baja-media · **Confianza:** alta<br>
**Archivo:** `init_sdk.sh:1-8`

No activa `set -e` ni comprueba resultados. Si falla `git submodule update`,
el `cd` o `get.py`, los comandos siguientes pueden ejecutarse en un contexto
incorrecto y el `cd` final puede dejar código de salida cero, reportando éxito
falso.

**Corrección requerida:** activar manejo estricto de errores y validar
prerrequisitos/paths; mostrar claramente qué paso falló.

### F-22 — Media/baja: la interfaz no aplica correctamente clases visuales de estado y accesibilidad

**Severidad:** baja-media · **Confianza:** alta<br>
**Archivos:** `static/js/main.js:626-638, 716-738, 778-790`;
`static/index.html:328-334`; `static/css/style.css:329-333, 520-570`

El renderer genera clases `normal`, `alarm`, etc., pero CSS define varias como
`.sector-card.status-*`; el nombre `no-response` del JS tampoco coincide con el
estado del modelo `no_response`. Las tarjetas no reciben los estilos previstos
para alarma/ausencia de respuesta. El contenedor HTML usa `sectors-cards` pero
CSS define la grilla como `sectors-grid`. El atributo accesible
`aria-valuenow` del medidor permanece fijo en 0 aunque el ancho visual cambie.

**Corrección requerida:** normalizar un único mapa de estados/clases compartido
por renderer y CSS; actualizar el valor ARIA junto con la lectura y comprobar
la interfaz con los estados límite.

### F-23 — Baja: las instrucciones y documentación operativa describen comportamientos que ya no coinciden con el código

**Severidad:** baja · **Confianza:** alta<br>
**Archivos:** `README.md:16-17`; `AGENTS.md:7-8`; `firmware/README.md:7-10,
45-55`; `redes-ii-vault/Arquitectura/Simulacion y arquitectura del
sistema.md:15, 50-52, 91, 245-246`; `redes-ii-vault/funcionalidades-pendientes.md:37-43`

README, AGENTS y arquitectura llaman determinista al simulador pese a su
aleatoriedad. La arquitectura dice que el dashboard consulta cada segundo,
pero el cliente programa polling cada cinco segundos. `firmware/README.md`
enumera `node_station` sin documentar `node_bus` ni el código `nano_node`, y
presenta métricas de build de una fecha anterior. Los planes de 2026-10-03
están marcados históricos, pero conservan referencias “24/24 tests OK”; no
deben leerse como verificación del checkout actual. Las instrucciones de build
del firmware no cubren el código RS485/Nano presente en el checkout. Estos
desajustes aumentan el riesgo de seguir pasos que ya no representan el sistema.

**Corrección requerida:** actualizar la documentación después de acordar y
validar el comportamiento; etiquetar resultados históricos con su revisión y
fecha exactas. Mantener explícito que el simulador no define la política física
de alarma.

### F-24 — Baja: el Makefile y el backup de Xmake son configuraciones heredadas

**Severidad:** baja · **Confianza:** alta<br>
**Archivos:** `Makefile.:1-5`; `xmake.lua.backup:1-3`

`Makefile.` apunta a `src/server.cpp` y a `lib/makeEspArduino`, rutas que no
corresponden a la estructura actual; el build soportado está configurado en el
`xmake.lua` de la raíz y el código propio vive en `firmware/`. El backup conserva
una configuración Xmake antigua con plataforma predeterminada `cross`. Aunque
ninguno se ejecuta por sí solo, ambos pueden inducir a usar un flujo roto o
recuperar una configuración obsoleta.

**Corrección requerida:** eliminar o identificar claramente estos archivos
como históricos si ya no se soportan; documentar un único flujo de build
vigente.

## Inventario por archivo

“Sin defecto confirmado” significa que en la revisión realizada no se confirmó
un defecto accionable; no garantiza ausencia de defectos ni sustituye pruebas
específicas. Los archivos generados, vendorizados o de datos de usuario se
excluyeron según el alcance anterior.

| Archivo | Resultado |
|---|---|
| `.gitignore` | Sin defecto confirmado en el alcance revisado. |
| `.gitmodules` | Sin defecto confirmado en el alcance revisado. |
| `AGENTS.md` | F-23: instrucciones de arquitectura/documentación desactualizadas respecto al simulador y bus. |
| `README.md` | F-23: afirma determinismo del simulador. |
| `Makefile.` | F-24: fuentes y dependencias con rutas heredadas. |
| `xmake.lua` | F-09, F-19: target Nano incorrecto; opción MCU y guardas de recursos. |
| `xmake.lua.backup` | F-24: configuración histórica que puede confundirse con la vigente. |
| `init_sdk.sh` | F-21: no propaga fallas de preparación de forma fiable. |
| `firmware/server.cpp` | F-07, F-10, F-13: recepción RS485 no integrada, autorización no aplicada, WebSocket anunciado pero deshabilitado. |
| `firmware/sensor.cpp` | F-06, F-14: respuesta incompatible con contrato y lista de nodos truncable. |
| `firmware/sensor.hpp` | Sin defecto confirmado en las declaraciones revisadas. |
| `firmware/node_bus.cpp` | F-07, F-08, F-20: recepción no integrada, polling/turnaround y lectura de CRC. |
| `firmware/node_bus.hpp` | F-07: falta declarar la rutina de recepción. |
| `firmware/protocol.h` | Sin defecto confirmado aislado; su contrato no está cubierto por pruebas compartidas (F-17, F-20). |
| `firmware/nano_node.cpp` | F-08, F-09: ID fijo, tiempos incompatibles con maestro y excluido del target AVR. |
| `firmware/node_station.cpp` | F-09: el target Nano compila el blink en vez del nodo RS485. |
| `firmware/tools/flasher.py` | F-18: fallback ambiguo a ESP8266 y selección de artefacto AVR potencialmente obsoleto. |
| `firmware/README.md` | F-23: lista de fuentes/targets y métricas de build desactualizadas. |
| `simulator/__init__.py` | Sin defecto confirmado; archivo de paquete. |
| `simulator/model.py` | F-05, F-15, F-16: nombre de control, aleatoriedad/reset y nodos incoherentes. |
| `simulator/server.py` | F-02, F-10, F-11, F-13, F-16: helper ausente, auth no aplicada, proxy, WebSocket y payloads. |
| `shared/api_state.schema.json` | F-03, F-06: JSON inválido y contrato divergente. |
| `static/index.html` | F-04, F-22: elementos requeridos ausentes y clases/contenedor desalineados. |
| `static/js/main.js` | F-01, F-04, F-05, F-12, F-13, F-22: sintaxis, DOM, acción, renderizado no escapado, WebSocket y clases. |
| `static/css/style.css` | F-22: selectores de estado/grilla desalineados; sin soporte `prefers-reduced-motion` pese a animaciones continuas. |
| `tests/test_contract.py` | F-03, F-06, F-17: setup bloqueado por JSON y ausencia de validación del serializador C++. |
| `tests/test_simulator.py` | F-02, F-11, F-17: pruebas revelan ruta rota; no acotan proxy ni cubren protocolos críticos. |
| `redes-ii-vault/Arquitectura/Simulacion y arquitectura del sistema.md` | F-23: describe tiempos de polling/determinismo distintos del código. |
| `redes-ii-vault/funcionalidades-pendientes.md` | Sin defecto confirmado de política; mantiene correctamente pendientes las decisiones físicas. Requiere reflejar integración RS485/targets una vez acordada. |
| `redes-ii-vault/plan migracion 2 capas.md` | Sin defecto confirmado en las secciones revisadas; documento de planificación. |
| `Documentacion/planes/2026-10-03-correctir-divergencia-firmware-contrato.md` | F-23: resultados de pruebas históricos; no aplican automáticamente al checkout actual. |
| `Documentacion/planes/2026-10-03-xmake-refactoring-plan.md` | F-23: plan histórico que debe distinguirse de la configuración actual. |
| `static/img/plano_escuela.png` | Asset gráfico; no se realizó auditoría de contenido visual ni licencia. |
| `static/css/pico.min.css`, `static/js/mustache.min.js` | Dependencias de terceros minificadas; excluidas del análisis de implementación. |

El archivo `Documentacion/Redes II - Servidor web - Informe del proyecto.md`
se excluyó por ser un documento académico extenso, no fuente operativa. Los
archivos bajo `firmware/.xmake/` son estado/metadatos generados del build local
y no se trataron como configuración mantenida.

## Validaciones ejecutadas

| Comando/verificación | Resultado |
|---|---|
| `python3 -m unittest discover -s tests -v` | **Falló:** 19 pruebas; 12 pasaron y 7 dieron error. El esquema no parsea y el handler HTTP cierra la conexión por `_request_path()` inexistente. |
| `node --check static/js/main.js` | **Falló:** sintaxis inválida en la línea 671; se confirmó otro literal inválido en la línea 720. |
| `python3 -m json.tool shared/api_state.schema.json` | **Falló:** `Extra data` en la línea 252. |
| `python3 -m py_compile` sobre módulos Python revisados | **Pasó.** |
| `xmake f -p linux --toolchain=xtensa-lx106-elf && xmake build server` | **Pasó**, sin flashear hardware: RAM 30.848/80.192 (38%), IRAM 60.287/65.536 (91%), flash 303.556/1.048.576 (28%). |
| Build AVR (`xmake f -p linux --toolchain=avr` y target `node_station`) | **No comprobable:** no están disponibles `avr-gcc`, `avr-g++`, `avr-objcopy` ni `avrdude`. |
| `cppcheck`, `clang-tidy`, `scan-build` | No instalados; no se ejecutó análisis estático con esas herramientas. |
| Hardware físico | No probado. El build Xtensa no valida el cableado, temporización del bus ni funcionamiento real de la placa. |

## Orden recomendado para la reestructuración

1. Restaurar artefactos y rutas que permitan ejecutar verificaciones: JSON
   válido, JavaScript parseable y handler HTTP operativo.
2. Definir el contrato canónico de `/api/state` y alinear serializadores,
   modelo y vistas; agregar pruebas que cubran firmware en host.
3. Decidir el alcance del dashboard y alinear HTML/JS/CSS/API; eliminar
   referencias a controles inexistentes y proteger el renderizado de datos.
4. Definir protocolo RS485 y configuración física antes de cambiar tiempos o
   comportamiento de alarma; después corregir parser, scheduler, direcciones,
   turnaround y target Nano.
5. Resolver seguridad antes de exponer servicios: autorización servidor,
   secretos, política del proxy y canal de transporte.
6. Corregir selección de builds/flasher, límites de recursos y documentación;
   ejecutar pruebas AVR y pruebas en hardware por separado.

Los hallazgos sobre autenticación, proxy, XSS y el bus deben reevaluarse después
de las decisiones de alcance, exposición y hardware. No se deben implementar
umbrales ni una política física de alarma a partir de estados demostrativos del
simulador.
