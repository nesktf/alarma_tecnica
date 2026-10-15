# Estado Actual del Proyecto: Alarma Técnica Distribuida

**Fecha:** 2026-10-06  
**Cátedra:** Redes II  
**Objetivo:** Monitorización, telemetría y dashboard distribuido para institución educativa.

---

## 1. Resumen Ejecutivo

El proyecto se estructura en una **arquitectura de dos capas** separadas por una barrera estricta de compilación y flasheo:

1. **Capa Firmware (`firmware/`)**:
   - Único código que se compila y flashea en el microcontrolador físico **NodeMCU ESP8266** (Xtensa LX106 @ 80 MHz, 4 MB flash).
   - Monta el sistema de archivos **LittleFS**, inicializa la conexión Wi-Fi en modo estación y ejecuta un servidor web HTTP en el puerto 80.
   - Sirve la interfaz web estática de producción (`firmware/static/`) empaquetada directamente en la flash LittleFS (3 pestañas de monitoreo, solo lectura, sin elementos de simulación ni controles de prueba).
   - Realiza muestreo del sensor ultrasónico HC-SR04 (Trig en `D6/GPIO12`, Echo en `D5/GPIO14`) con timeout de 30 ms y cadencia de 1 s.
   - Expone la instantánea de estado del sistema mediante `GET /api/state`.
   - Incorpora el firmware esclavo para Arduino Nano (`firmware/nano_node.cpp`) y el protocolo de bus RS485 (`firmware/protocol.h`).

2. **Capa Simulador (`simulator/`)**:
   - Servidor HTTP local en Python 3.10+ (`127.0.0.1:8080`) sin dependencias externas (`pip install` no requerido).
   - Diseñado con un reloj inyectable para reproducir de forma determinista la cadencia de muestreo, fallos de Wi-Fi, fallos de LittleFS, timeouts de sensor y escenarios de la máquina de estados de alarma.
   - Provee la API `POST /api/controls` (restringida al simulador, no expuesta en el firmware) para inyección de escenarios interactivos desde el navegador.
   - Sirve el frontend de desarrollo (`static/`) con 4 pestañas incluyendo el Banco de Pruebas.

Ambas capas comparten el **mismo contrato observable JSON** definido en `shared/api_state.schema.json`.

---

## 2. Inventario de Componentes y Fuentes

| Módulo / Archivo | Rol en el Sistema |
|---|---|
| `firmware/server.cpp` | Punto de entrada del firmware ESP8266 (`setup()` y `loop()`), LittleFS, Wi-Fi y servidor HTTP. |
| `firmware/sensor.cpp` / `.hpp` | Medición del sensor HC-SR04 y serializador del JSON de telemetría hacia el contrato. |
| `firmware/protocol.h` | Estructuras de trama binaria RS485 (5 bytes consulta, 6 bytes respuesta) y cálculo CRC-16/CCITT-FALSE. |
| `firmware/nano_node.cpp` | Firmware para nodos de área en Arduino Nano (ATmega328P) con DIP switch, PIR y pulsador de pánico. |
| `firmware/node_bus.cpp` / `.hpp` | Rutinas maestras de polling round-robin RS485 para el bus de campo. |
| `firmware/static/` | **Frontend limpio de producción** empaquetado en LittleFS (`index.html`, `js/main.js`, `css/style.css`): solo lectura, 3 pestañas (Mapa, Grilla, NodeMCU), sin controles de prueba locales. |
| `simulator/model.py` | Modelo de estado del sistema, simulación de nodos, lógica de alarma y telemetría de $V_{bus}$. |
| `simulator/server.py` | Servidor HTTP de desarrollo, manejo de endpoints `/api/state`, `/api/controls`, `/api/proxy` y WebSocket. |
| `shared/api_state.schema.json` | Contrato JSON schema formal (Draft 7/2020-12) para `GET /api/state`. |
| `static/` | **Frontend de desarrollo y simulación** (4 pestañas: Mapa, Grilla, NodeMCU y Banco de Pruebas `#tabTests` con inyección de fallas y escenarios). |
| `tests/test_contract.py` | Pruebas de conformidad de contrato JSON y validación cruzada del CRC16 binario C++/Python. |
| `tests/test_simulator.py` | Pruebas unitarias del modelo de simulación, tiempos, validación de controles y endpoints HTTP. |
| `xmake.lua` | Archivo de build único en la raíz para toolchains Xtensa LX106 (`server`) y AVR (`node_station`). |

---

## 3. Estado de la Interfaz de Usuario (Frontend)

La interfaz web compartida cuenta con 4 vistas principales:

- **Pestaña 1: Mapa de Sectores**:
  - Plano SVG interactivo del colegio con 9 áreas (Zonas 1 a 5 actuales, Zonas 6 a 9 proyectadas).
  - Modo proyectado oculto por defecto para mantener la legibilidad inicial del plano.
  - Semáforo central en barra superior con 4 estados: `DESARMADO` (verde), `ARMANDO` (amarillo con cuenta regresiva de 60 s y beeps), `ARMADO` (azul) y `ALARMA` (rojo con sirena modulada interactiva).
  - Inspector lateral de sector con atajos directos para simular movimiento PIR, conmutar pulsador de emergencia y desarmar/armar la alarma central sin cambiar de pestaña.
  - Sintetizador de audio en JavaScript puro ([`AlarmAudioSynthesizer`](file:///home/chanchito/proyectos/redes-ii/static/js/main.js)) mediante Web Audio API con osciladores LFO y desconexión segura `try/finally`.

- **Pestaña 2: Grilla y Eventos**:
  - Tarjetas de estado individuales para cada sector en grilla CSS responsiva (`.sectors-grid`).
  - Registro histórico tabular con timestamps, nivel de severidad (`info`, `warning`, `critical`), códigos normalizados y detalle descriptivo.

- **Pestaña 3: Dispositivo NodeMCU**:
  - Esquema visual de la placa ESP8266 con asignación de pines GPIO (`D5 Echo`, `D6 Trig`).
  - Indicadores de conexión Wi-Fi, estado de montaje de LittleFS, límites de CPU/Flash y tiempo de actividad.
  - Lectura en vivo del sensor ultrasónico con indicador accesible (`role="meter"`, atributo `aria-valuenow`).

- **Pestaña 4: Banco de Pruebas (Preview en Simulador)**:
  - Exclusivo del simulador (se oculta automáticamente en hardware real cuando `simulator: false`).
  - Botones de forzado de máquina de estados (`DESARMAR`, `ARMANDO`, `FORZAR ARMADO`, `ALARMA GENERAL`).
  - Control de tensión de alimentación $V_{bus}$ con presets (Normal 12.4 V, Alerta 11.4 V, Crítico 10.5 V).
  - Botón de Reset Total determinista.

---

## 4. Métricas de Compilación y Memoria

### Build de Firmware ESP8266 (Xtensa LX106 @ 80 MHz)
Invocado desde la raíz del repositorio:
```sh
xmake f -p linux --toolchain=xtensa-lx106-elf && xmake
```

- **RAM (Data + Rodata + BSS):** 31.232 / 80.192 bytes (**38%** utilizado).
- **IRAM (Instrucciones en RAM):** 60.287 / 65.536 bytes (**91%** utilizado — margen estrecho).
- **Flash ROM (IROM):** 304.036 / 1.048.576 bytes (**28%** utilizado).
- **Imagen LittleFS (`build/littlefs.bin`):** Generada y empaquetada con éxito conteniendo todos los assets de `static/` (HTML, CSS, JS, imágenes).

---

## 5. Resultados de Pruebas Automatizadas

Ejecución de la suite completa de pruebas:
```sh
python3 -m unittest discover -s tests -v
```

**Resultado:** **30 de 30 pruebas superadas (100% OK)** en 1.05 segundos.

- **Pruebas de Contrato (`test_contract.py`)**:
  - Validación del esquema `shared/api_state.schema.json` como documento JSON válido único.
  - Presencia y tipos de claves obligatorias del contrato base.
  - Validación de extensiones `preview_only` exclusivas del simulador.
  - Validación del serializador C++ de `firmware/sensor.cpp` contra el esquema.
  - Verificación cruzada del protocolo binario RS485 (`protocol.h`) y cálculo de CRC-16/CCITT-FALSE.
- **Pruebas de Simulación y HTTP (`test_simulator.py`)**:
  - Determinismo del modelo tras `reset_all`.
  - Cadencia estricta de muestreo de 1 segundo.
  - Distinción entre distancia válida y timeout de Echo (no reporta `0 cm`).
  - Máquina de estados: temporización de 60 s de armado y disparo ante movimiento.
  - API HTTP: `POST /api/controls`, rechazo de payloads inválidos o fuera de rango, proxy seguro y manejo de rutas.

---

## 6. Correspondencia con el Informe Académico (`Documentacion/...`)

El sistema implementa fielmente los principios de la cátedra:
1. **Lógica de disparo supervisada:** $\text{ALARMA} = \text{ARM} \cdot (M_a \lor T_a) \lor E_a \lor (V_{bus} < V_{disp})$.
2. **Distribución de 9 áreas:** Zonas 1 a 5 activas y 6 a 9 proyectadas, con pulsadores de emergencia en Central, Taller y Vicedirección.
3. **Telemetría de alimentación:** Umbrales idénticos a los calculados (11.8 V aviso, 11.0 V disparo).
4. **Protocolo RS485:** Formato 8N1 a 9600 baud, tramas binarias de 5/6 bytes y CRC16.
5. **Filosofía de servidor:** El NodeMCU actúa como interfaz de visualización y telemetría sin atribuirse la toma de decisiones físicas ni el accionamiento del timbre en el hardware real.

---

## 7. Próximos Pasos (Backlog Técnico Priorizado)

1. **Integración RS485 en bucle principal:** Enlazar la llamada de polling de `node_bus.cpp` dentro del `loop()` de `firmware/server.cpp`.
2. **Confirmación en hardware de laboratorio:** Verificar el cableado físico del sensor HC-SR04 y el divisor resistivo para el pin Echo (5 V a 3.3 V).
3. **Anillos binarios en LittleFS:** Implementar en C++ la persistencia de eventos compactos de 16 bytes con rotación de segmentos para preservar la vida útil de la flash.
