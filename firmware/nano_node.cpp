/**
 * Firmware para Arduino Nano (ATmega328P) — Nodo esclavo del bus RS485
 *
 * Protocolo de comunicación:
 *   Maestro (ESP8266) envía:  1 byte  = 0xB0 | node_id
 *   Esclavo (Nano) responde:   7 bytes = sensor_packet_t (sync + node_id + distance + status + crc)
 *
 * El Nano lee su sensor ultrasónico y responde con la medición actual.
 * El cálculo de CRC-16-CCITT es idéntico al del ESP8266 (protocolo.h).
 */

#include <stdint.h>

// ---------------------------------------------------------------------------
// Configuración del nodo
// ---------------------------------------------------------------------------

#ifndef NANO_NODE_ID
#define NANO_NODE_ID        0x01        // ID único del nodo (1..9, configurable vía build flag)
#endif

#define NANO_TRIG_PIN       9
#define NANO_ECHO_PIN       10
#define NANO_SOUNVEL        0.034f      // cm/us

#ifndef NANO_DE_PIN
#define NANO_DE_PIN         2           // Pin DE/!RE opcional del MAX485 en Nano
#endif

// Bitmask de estado del nodo
#define NANO_STATUS_ALARM   0x01        // Bit 0: condición de alarma
#define NANO_STATUS_ERROR   0x02        // Bit 1: error de sensor (timeout)
#define NANO_STATUS_HB      0x04        // Bit 2: heartbeat (se envía en cada paquete)

static float cached_distance_cm = -1.0f;
static unsigned long last_sample_time = 0;

// ---------------------------------------------------------------------------
// CRC-16-CCITT (XModem) — idéntico al del ESP8266
// ---------------------------------------------------------------------------

static uint16_t calculate_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Lectura del sensor ultrasónico
// ---------------------------------------------------------------------------

static float read_ultrasonic() {
    digitalWrite(NANO_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(NANO_TRIG_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(NANO_TRIG_PIN, LOW);

    unsigned long duration = pulseIn(NANO_ECHO_PIN, HIGH, 30000);
    if (!duration) return -1.0f; // timeout
    return static_cast<float>(duration) * NANO_SOUNVEL / 2.0f;
}

static void poll_ultrasonic_async() {
    unsigned long now = millis();
    if (now - last_sample_time >= 500) { // muestreo cada 500 ms
        last_sample_time = now;
        cached_distance_cm = read_ultrasonic();
    }
}

// ---------------------------------------------------------------------------
// Envío de la trama binaria
// ---------------------------------------------------------------------------

static void send_packet(uint16_t distance_cm, uint8_t status) {
    uint8_t raw[7];
    raw[0] = 0xAA;                    // sync
    raw[1] = NANO_NODE_ID;            // node_id
    raw[2] = distance_cm & 0xFF;      // distance (LSB)
    raw[3] = (distance_cm >> 8) & 0xFF; // distance (MSB)
    raw[4] = status;                  // status

    // CRC sobre los primeros 5 bytes
    uint16_t crc = calculate_crc16(raw, 5);
    raw[5] = crc & 0xFF;              // CRC (LSB)
    raw[6] = (crc >> 8) & 0xFF;       // CRC (MSB)

    digitalWrite(NANO_DE_PIN, HIGH);
    Serial.write(raw, 7);
    Serial.flush();
    digitalWrite(NANO_DE_PIN, LOW);
}

// ---------------------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------------------

void setup() {
    pinMode(NANO_TRIG_PIN, OUTPUT);
    pinMode(NANO_ECHO_PIN, INPUT);
    pinMode(NANO_DE_PIN, OUTPUT);
    digitalWrite(NANO_DE_PIN, LOW);
    Serial.begin(9600);
}

void loop() {
    poll_ultrasonic_async();

    // Esperar comando de polling del maestro
    if (Serial.available() >= 1) {
        uint8_t cmd = Serial.read();

        // Verificar que es un comando de polling válido para este nodo
        if ((cmd & 0xF0) == 0xB0 && (cmd & 0x0F) == NANO_NODE_ID) {
            float dist = cached_distance_cm;
            uint16_t distance_cm = (dist >= 0.0f) ? static_cast<uint16_t>(dist) : 0;
            uint8_t status = NANO_STATUS_HB;

            if (dist < 0.0f) {
                status |= NANO_STATUS_ERROR; // timeout del sensor
            }

            send_packet(distance_cm, status);
        }
    }
}
