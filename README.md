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
- `SERVER_IP`: Dirección IP estática del servidor web (por defecto `192.168.0.53`).
- `SERVER_GATEWAY`: Puerta de enlace predeterminada.
- `SERVER_MASK`: Máscara de subred.
- `SERVER_LOGIN_PASS`: Contraseña de acceso al panel web (se compila como hash SHA-256).
- `MASTER_KEYPASS_PASSCODE`: Clave numérica para el teclado físico del nodo maestro.

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

Graba el firmware en la placa seleccionando el puerto serial correspondiente:

```bash
# Grabar nodo maestro o esclavo
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

## Simulador Local

El proyecto incluye un simulador en Python que emula el comportamiento de la central, el bus RS485 y los sensores, sirviendo la misma interfaz web estática sin necesidad de hardware conectado.

### Iniciar el Simulador

```bash
python3 -m simulator.server
```

Abre automáticamente el navegador en `http://127.0.0.1:8080`.

### Opciones del Simulador

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
