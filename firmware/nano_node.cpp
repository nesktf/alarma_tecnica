#include <stdint.h>
#include <Arduino.h>
#include "config.h"
#include "protocol.h"

#ifndef NANO_NODE_ID
#define NANO_NODE_ID SEC_AUTOMOTORES
#endif

#ifndef HAS_PANIC_BUTTON
#if (NANO_NODE_ID == SEC_TALLER || NANO_NODE_ID == SEC_VICEDIRECCION)
#define HAS_PANIC_BUTTON 1
#else
#define HAS_PANIC_BUTTON 0
#endif
#endif

static float cached_distance_cm = -1.0f;
static unsigned long last_sample_time = 0;
static bool latched_motion = false;
static bool latched_panic = false;
static bool sensor_timeout_error = false;

static float read_ultrasonic() {
    digitalWrite(SLAVE_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(SLAVE_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(SLAVE_TRIG_PIN, LOW);

    unsigned long duration = pulseIn(SLAVE_ECHO_PIN, HIGH, 30000UL);
    if (!duration) {
        return -1.0f;
    }
    return (float)duration * 0.034f / 2.0f;
}

static void poll_inputs() {
    unsigned long now = millis();
    if (now - last_sample_time < 100) return;
    last_sample_time = now;

    float dist = read_ultrasonic();
    if (dist >= 0.0f) {
        cached_distance_cm = dist;
        sensor_timeout_error = false;
        if (dist < (float)MOTION_DISTANCE_THRESHOLD_CM) {
            latched_motion = true;
        }
    } else {
        sensor_timeout_error = true;
    }

    #if HAS_PANIC_BUTTON
    if (digitalRead(SLAVE_PANIC_PIN) == LOW) {
        latched_panic = true;
    }
    #endif
}

static void send_response_packet() {
    rs485_packet_t pkt;
    pkt.sync = RS485_SYNC_BYTE;
    pkt.node_id = (uint8_t)NANO_NODE_ID;
    pkt.subnode_count = 1;

    uint16_t dist = (cached_distance_cm >= 0.0f) ? (uint16_t)cached_distance_cm : 0;
    pkt.distance_cm = dist;

    uint8_t flags = STATUS_FLAG_HB;
    if (latched_motion) flags |= STATUS_FLAG_MOTION;
    if (latched_panic)  flags |= STATUS_FLAG_PANIC;
    if (sensor_timeout_error) flags |= STATUS_FLAG_ERROR;
    pkt.status_flags = flags;

    pkt.crc = calculate_crc16((const uint8_t *)&pkt, 6);

    digitalWrite(SLAVE_RS485_DE_PIN, HIGH);
    delayMicroseconds(50);
    Serial.write((const uint8_t *)&pkt, sizeof(pkt));
    Serial.flush();
    digitalWrite(SLAVE_RS485_DE_PIN, LOW);

    latched_motion = false;
    latched_panic = false;
}

void setup() {
    pinMode(SLAVE_TRIG_PIN, OUTPUT);
    pinMode(SLAVE_ECHO_PIN, INPUT);
    pinMode(SLAVE_RS485_DE_PIN, OUTPUT);
    digitalWrite(SLAVE_RS485_DE_PIN, LOW);

    #if HAS_PANIC_BUTTON
    pinMode(SLAVE_PANIC_PIN, INPUT_PULLUP);
    #endif

    Serial.begin(9600);
}

void loop() {
    poll_inputs();

    while (Serial.available() > 0) {
        uint8_t cmd = (uint8_t)Serial.read();
        if ((cmd & 0xF0) == RS485_POLL_CMD_PREFIX && (cmd & 0x0F) == NANO_NODE_ID) {
            send_response_packet();
        }
    }
}
