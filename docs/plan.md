# Alarma tecnica
This is a project for implementing an alarm system in a highschool.

## Design
The main goal is to implement a system that picks sensor data from a series of nodes located in
different rooms of the building, forward them through a common data bus, process them in a central
processing node, and forward them to a web dashboard.

We use a bus architecture with three main components:
1. Web client, hosted on a NodeMCU ESP8266
2. Master node, hosted on an Arduino Nano
3. A series of slave nodes, hosted on Arduino nanos

The tasks for each one are numbered as follows:

### Slave nodes
We have four sections:
- "Automotores"
- "Jefatura de Taller"
- "Vicedirección"
- "Informática"

These slave nodes can also connect to sub nodes (with arduino nanos) using basic serial
comunication. Each sub node has at least a PIR infrared movement sensor that activates the alarm,
some of them can have an emergency button to activate the alarm manually.

The slave nodes are connected to the master node using a network of RS485 using a bus
configuration. The main idea is that the master node polls every single slave node in a round robin
setting in a span of ~5 minutes (should be configurable in the firmware), the slave nodes respond
with the movement status (last movement is cached until the next polling) and connection status.
The master node should turn on the alarm if either:
- We have movement three times in a row (configurable in firmware)
- Any node is disconnected and does not respond in the span of 5 minutes (only if the node is not
  disabled by the master node).

For now, assume the following:
- We are using an HC-SR04 ultrasonic distance sensor to emulate the infrared sensor
- The only subnode connected on each node is hosted at the slave itself. Keep the design to add
  subnodes in the future. This means the slave node has connections to the sensor and the button
- The button is optional, this should be configurable in the firmware compilation settings.

### Master node
The master node is the bridge between the web client and the node network. It also has some sensors
of its own, the same infrared motion sensor and emergency button which do the same. In the
dashboard it appears as an extra node in the network called "Área operativa".

This node has the following jobs:
- Have a motion sensor and emergency button on its own.
- Poll the status of all the other nodes in the network, forward it to the NodeMCU.
- Have an activation sequence with three LEDs:
    - Green LED ON: Alarm deactivated
    - Yellow LED ON (Flashing every couple of secs): Alarm armed
    - Red LED ON: Alarm is on
- Control an actual alarm, just forward a digital signal to turn on a transistor.
- Recieve a password from a keypad to turn the alarm on or off + a signal to turn on or off each
  slave node section.

The Keypad is a standard 16 button keypad, with buttons from 0-9, *, #, A, B, C, D. Has 8
terminals, so it probably encodes every button input as a byte packet. The user inputs the passcode
(configurable in firmware) then presses a button to turn on or off the sensors in a specific
section:
- '*': toggles all sections (sets on or off to all of them)
- '#': toggles "Área operativa"
- 'A': toggles "Automotores"
- 'B': toggles "Vicedirección"
- 'C': toggles "Informática"
- 'D': toggles "Jefatura de Taller"

The node should probably inform the user of success or failure with the LEDs, think of some color
combination for this.

Finally, the master node is connected with the NodeMCU using a serial connection and a 3.3v-5v
logic converter. This connection should forward the node network sensor and connection data and
some controls from the dashboard side. See the web client for more info.

### Web client
We use a NodeMCU to host a web page and a couple of APIs, to control the system activation and to
control some status LEDs.

The user can log into a web dashboard, here they can see:
- The status of every section in the network, with subnodes in each one. This uses a map of the
  building with some colored overlays: Blue for active, Green for alarm armed, Red for alarm
  activated, white for disabled.
- Controls to turn on or off each node section.
- Controls to manually ping each section in the network.

The login screen should be required before doing anything in the dashboard, the NodeMCU stores a
sha256 hash for some key defined at firmware compilation time and checks using that when the user
tries to log in.

The map is stored at `static/img/plano_escuela.png`. Use simple overlays over the image to show the
status. Check the current `index.html` and steal from that. Use the same style that we set in the
original `index.html`, remove everything not necessary.

When the user selects a section they should see the status for all the sub nodes (with the same
color standard that we were using).

When the alarm turns on, the web client should also play a simple alarm sound. Use the JS sound api
for this.

The NodeMCU controls five LEDs that signal the status for each sector in the network. LED turned on
means active and connected, LED turned off means disconnected or inactive.

## Tech stack
Arduino standard libraries for the nanos and the NodeMCU, located at `firmware/lib/`. Check the old
`xmake.lua` for using it as a base to compile everything, remove things that are not necessary.

The NodeMCU has a LittleFS partition with the webpage. It is connected to a local network and
potentially exposed to the internet.

Use simple HTML, CSS, vanilla JS to make the web client. Uses Mustache.js for client side
templates and Pico.CSS for styling, do not pull extra dependencies. The code should fit in the 2MiB
LittleFS that will be flashed in the NodeMCU.

## Cleaning things up
We have some things set up already, but most of it is useless for the actual project. Check the
mardown file at `docs/estado-actual-del-proyecto.md` for more info. Keep only the strictly
necessary things like the WiFi credential parser, parts of the xmake config, etc.

We had a NodeMCU server simulator in `simulator/` with some tests in `tests/`, check the code and
see if it can be useful to emulate the NodeMCU behaviour with this new specification, otherwise
discard it.
