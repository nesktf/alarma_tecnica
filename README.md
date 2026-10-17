# Alarma Técnica

Sistema distribuido de alarma técnica y monitoreo perimetral para instituciones educativas. Combina un coordinador central basado en Arduino Nano, nodos sensores esclavos comunicados por bus diferencial RS485 y una pasarela web basada en NodeMCU ESP8266 con almacenamiento LittleFS.

## Arquitectura

- **Servidor Gateway (NodeMCU ESP8266):** Expone la interfaz web REST en LittleFS, maneja la autenticación por token/hash SHA-256 y puentea comandos hacia el coordinador central por puerto serial.
- **Nodo Coordinador (Arduino Nano Maestro):** Gestiona la máquina de estados de alarma, escanea el teclado matricial 4x4, lee el sensor local y sondea a los nodos esclavos por RS485.
- **Nodos Esclavos (Arduino Nano):** Nodos distribuidos con transceptores MAX485 y sensores ultrasónicos HC-SR04 para detección de presencia y botones de emergencia.

## Requisitos

- [xmake](https://xmake.io) (v2.8+)
- Python 3.8+
- Toolchain AVR (`avr-gcc`, `avr-g++`, `avr-objcopy`) para compilar los nodos Arduino Nano.
- El toolchain Xtensa (`xtensa-lx106-elf`) para ESP8266 se incluye empaquetado en el repositorio (`firmware/lib/esp8266/tools/`).

## Configuración

Copia la plantilla de variables de entorno y ajusta las credenciales de red y acceso:

```bash
cp .env.example .env
```

Parámetros en `.env`:
- `WIFI_SSID`: Nombre de la red Wi-Fi para el NodeMCU.
- `WIFI_PASS`: Contraseña de la red Wi-Fi.
- `SERVER_IP`: Dirección IP estática del servidor web.
- `SERVER_GATEWAY`: Puerta de enlace predeterminada.
- `SERVER_MASK`: Máscara de subred.
- `SERVER_LOGIN_PASS`: Contraseña de acceso al panel web.
- `MASTER_KEYPASS_PASSCODE`: Clave numérica para el teclado físico del nodo maestro.

## Conexiones de Hardware y Pinout

### Enlace Bus RS485 (Módulos MAX485)

En cada transceptor MAX485 se deben puentear entre sí los pines **`DE`** y **`RE`** para controlar transmisión con un único pin digital:

| Pin MAX485 | Función | Arduino Nano Maestro | Arduino Nano Esclavo |
| :--- | :--- | :--- | :--- |
| **`RO`** | Salida del receptor (hacia **`RX`**) | **`D2`** (`MASTER_RS485_RX_PIN`) | **`RX` (`D1`)** (Serial Hardware RX) |
| **`DI`** | Entrada del transmisor (desde **`TX`**) | **`D3`** (`MASTER_RS485_TX_PIN`) | **`TX` (`D0`)** (Serial Hardware TX) |
| **`DE` + `RE`** | Habilitación TX/RX (puenteados) | **`D4`** (`MASTER_RS485_DE_PIN`) | **`D2`** (`SLAVE_RS485_DE_PIN`) |
| **`A` / `B`** | Par diferencial RS485 | Conectar **`A` con `A`** y **`B` con `B`** del bus | Conectar **`A` con `A`** y **`B` con `B`** del bus |
| **`VCC` / `GND`** | Alimentación | `5V` / `GND` común | `5V` / `GND` común |

> **Nota al grabar un Nano Esclavo por USB:** Desconectar temporalmente el cable **`RO` $\rightarrow$ `RX` (`D1`)** durante el flasheo para que el MAX485 no retenga la línea serie de programación.

---

### Nodo Coordinador: Arduino Nano Maestro (Sector 1 - Área Operativa)

| Pin Arduino Nano | Constante en Firmware | Periférico / Conexión | Dirección / Notas |
| :--- | :--- | :--- | :--- |
| **`TX` (`D0`)** | Hardware `Serial` (TX) | **`RX` (`GPIO3`)** del NodeMCU ESP8266 | Salida UART 115200 bps (vía divisor/conversor 5V $\rightarrow$ 3.3V) |
| **`RX` (`D1`)** | Hardware `Serial` (RX) | **`TX` (`GPIO1`)** del NodeMCU ESP8266 | Entrada UART 115200 bps (vía conversor 3.3V $\rightarrow$ 5V) |
| **`D2`** | `MASTER_RS485_RX_PIN` | MAX485 **`RO`** | Entrada `SoftwareSerial` RX (9600 bps) |
| **`D3`** | `MASTER_RS485_TX_PIN` | MAX485 **`DI`** | Salida `SoftwareSerial` TX (9600 bps) |
| **`D4`** | `MASTER_RS485_DE_PIN` | MAX485 **`DE` + `RE`** | Salida control dirección RS485 (`HIGH` = TX, `LOW` = RX) |
| **`D5`** | `MASTER_SIREN_PIN` | Sirena general | Salida digital (`HIGH` = activa) |
| **`D6`** | `MASTER_LED_GREEN_PIN` | LED Verde central | Salida digital (Desarmado / clave correcta) |
| **`D7`** | `MASTER_LED_YELLOW_PIN` | LED Amarillo central | Salida digital (Armando / armado) |
| **`D8`** | `MASTER_LED_RED_PIN` | LED Rojo central | Salida digital (Pre-alarma / alarma / clave inválida) |
| **`D9`** | `MASTER_TRIG_PIN` | Sensor ultrasónico HC-SR04 **`TRIG`** | Salida pulso de disparo (Sector 1 local) |
| **`D10`** | `MASTER_ECHO_PIN` | Sensor ultrasónico HC-SR04 **`ECHO`** | Entrada pulso de eco (Sector 1 local) |
| **`D11`** | `MASTER_PANIC_PIN` | Pulsador de emergencia local | Entrada `INPUT_PULLUP` (activo en `LOW` hacia `GND`) |
| **`D12`** | `KEYPAD_ROW_1_PIN` | Teclado 4x4 **Fila 1** (`1, 2, 3, A`) | Salida barrido de fila |
| **`D13`** | `KEYPAD_ROW_2_PIN` | Teclado 4x4 **Fila 2** (`4, 5, 6, B`) | Salida barrido de fila |
| **`A0`** | `KEYPAD_ROW_3_PIN` | Teclado 4x4 **Fila 3** (`7, 8, 9, C`) | Salida barrido de fila |
| **`A1`** | `KEYPAD_ROW_4_PIN` | Teclado 4x4 **Fila 4** (`*, 0, #, D`) | Salida barrido de fila |
| **`A2`** | `KEYPAD_COL_1_PIN` | Teclado 4x4 **Columna 1** (`1, 4, 7, *`) | Entrada `INPUT_PULLUP` |
| **`A3`** | `KEYPAD_COL_2_PIN` | Teclado 4x4 **Columna 2** (`2, 5, 8, 0`) | Entrada `INPUT_PULLUP` |
| **`A4`** | `KEYPAD_COL_3_PIN` | Teclado 4x4 **Columna 3** (`3, 6, 9, #`) | Entrada `INPUT_PULLUP` |
| **`A5`** | `KEYPAD_COL_4_PIN` | Teclado 4x4 **Columna 4** (`A, B, C, D`) | Entrada `INPUT_PULLUP` |

---

### Nodos Sensores: Arduino Nano Esclavo (Sectores 2 a 5)

| Pin Arduino Nano | Constante en Firmware | Periférico / Conexión | Dirección / Notas |
| :--- | :--- | :--- | :--- |
| **`TX` (`D0`)** | Hardware `Serial` (TX) | MAX485 **`DI`** | Salida UART RS485 (9600 bps) |
| **`RX` (`D1`)** | Hardware `Serial` (RX) | MAX485 **`RO`** | Entrada UART RS485 (9600 bps) |
| **`D2`** | `SLAVE_RS485_DE_PIN` | MAX485 **`DE` + `RE`** | Salida control dirección RS485 (`HIGH` = TX, `LOW` = RX) |
| **`D9`** | `SLAVE_TRIG_PIN` | Sensor ultrasónico HC-SR04 **`TRIG`** | Salida pulso de disparo |
| **`D10`** | `SLAVE_ECHO_PIN` | Sensor ultrasónico HC-SR04 **`ECHO`** | Entrada pulso de eco |
| **`D11`** | `SLAVE_PANIC_PIN` | Pulsador de emergencia | Entrada `INPUT_PULLUP` (activo en `LOW` a `GND`; habilitado en Sector 3 Taller y Sector 4 Vicedirección) |

---

### Servidor Gateway: NodeMCU ESP8266

| Pin NodeMCU | GPIO ESP8266 | Constante en Firmware | Periférico / Conexión | Dirección / Notas |
| :--- | :--- | :--- | :--- | :--- |
| **`RX`** | `GPIO3` (`RXD0`) | Hardware `Serial` (RX) | **`TX` (`D0`)** del Nano Maestro | Entrada UART 115200 bps (usar conversor 5V $\rightarrow$ 3.3V) |
| **`TX`** | `GPIO1` (`TXD0`) | Hardware `Serial` (TX) | **`RX` (`D1`)** del Nano Maestro | Salida UART 115200 bps (usar conversor 3.3V $\rightarrow$ 5V) |
| **`D1`** | `GPIO5` | `NODEMCU_LED_SEC_OPERATIVA` | LED Sector 1 (Área Operativa) | Salida digital (`HIGH` cuando habilitado y en línea) |
| **`D2`** | `GPIO4` | `NODEMCU_LED_SEC_AUTOMOTORES` | LED Sector 2 (Automotores) | Salida digital (`HIGH` cuando habilitado y en línea) |
| **`D5`** | `GPIO14` | `NODEMCU_LED_SEC_TALLER` | LED Sector 3 (Jefatura de Taller) | Salida digital (`HIGH` cuando habilitado y en línea) |
| **`D6`** | `GPIO12` | `NODEMCU_LED_SEC_VICEDIRECCION` | LED Sector 4 (Vicedirección) | Salida digital (`HIGH` cuando habilitado y en línea) |
| **`D7`** | `GPIO13` | `NODEMCU_LED_SEC_INFORMATICA` | LED Sector 5 (Informática) | Salida digital (`HIGH` cuando habilitado y en línea) |
| **`GND`** | `GND` | Masa común | **`GND`** del Nano Maestro y fuentes | Referencia común obligatoria |

## Compilación

Compilar todos los objetivos del proyecto:

```bash
xmake
```

Compilar un objetivo individual:

```bash
# Servidor web NodeMCU ESP8266
xmake build server

# Nodo coordinador maestro
xmake build master_node

# Nodo sensor esclavo
xmake build slave_node
```

## Grabación en Placa (Flashing)

### NodeMCU ESP8266

Graba el firmware principal y el sistema de archivos web LittleFS:

```bash
# Grabar firmware del servidor
xmake flash -t esp8266 -p /dev/ttyUSB0

# Grabar imagen web LittleFS
xmake flash_fs -p /dev/ttyUSB0

# Grabar firmware y fs
xmake flash -t esp8266 -p /dev/ttyUSB0 -a
```

### Arduino Nano

Graba el firmware en la placa seleccionando el objetivo y puerto serial correspondiente:

```bash
# Grabar nodo coordinador maestro
xmake flash -t master_node -p /dev/ttyUSB0

# Grabar nodo sensor esclavo
xmake flash -t nano -p /dev/ttyUSB1
```

Opciones de grabación comunes:
- `-p <puerto>`: Especifica el puerto serial (por ejemplo `/dev/ttyUSB0`).
- `-b <baudrate>`: Tasa de baudios (57600 para bootloader antiguo, 115200 para Optiboot).
- `-B <old|new>`: Tipo de bootloader del Nano.

### Monitor Serial

```bash
xmake monitor -p /dev/ttyUSB0 -b 115200
```

## Simulador Local y Puente Serie USB

El proyecto incluye un servidor local en Python que sirve la interfaz web estática y puede operar tanto en modo 100% simulado como en **modo puente USB Serie** conectado directamente al Arduino Nano Maestro (reemplazando temporalmente al NodeMCU ESP8266).

### Iniciar el Simulador Virtual

```bash
python3 -m simulator.server
```

Abre automáticamente el navegador en `http://127.0.0.1:8080`.

### Iniciar en Modo Puente con el Arduino Nano Maestro

```bash
# Detectar automáticamente el puerto USB del Nano Maestro (115200 bps)
python3 -m simulator.server --serial-port auto

# O especificando el puerto serie manualmente
python3 -m simulator.server --serial-port /dev/ttyUSB0 --baud 115200
```

### Lector de Tramas Binarias a JSON

Para inspeccionar directamente por terminal las tramas binarias del Nano Maestro (`gw_state_packet_t`) o de un Nano Esclavo (`rs485_packet_t`):

```bash
# Escuchar tramas del Nano Maestro (115200 bps)
python3 simulator/serial_reader.py -p /dev/ttyUSB0

# Sondear y escuchar a un Nano Esclavo por USB (9600 bps, ID 2)
python3 simulator/serial_reader.py -m slave -p /dev/ttyUSB1 --poll-id 2
```

### Otras Opciones del Servidor

```bash
# Iniciar en modo terminal sin abrir el navegador
python3 -m simulator.server --no-open

# Iniciar en un puerto específico
python3 -m simulator.server --port 8000 --no-open

# Configurar IP virtual reflejada en el dashboard
python3 -m simulator.server --device-ip 192.168.0.53 --device-port 80 --no-open
```

### Pruebas Unitarias del Simulador

Ejecuta el conjunto de pruebas de contrato API y lógica de simulación:

```bash
python3 -m unittest discover -s simulator/tests -v
```

