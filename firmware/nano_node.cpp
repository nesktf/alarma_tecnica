#include "config.h"
#include "protocol.h"
#include <Arduino.h>
#include <stdint.h>

#include "node_sensor.hpp"

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

namespace {

float cached_distance_cm = -1.0f;
unsigned long last_sample_time = 0;
bool latched_motion = false;
bool latched_panic = false;
bool sensor_timeout_error = false;

void poll_inputs() {
  const auto now = millis();
  if (now - last_sample_time < 100) {
    return;
  }
  last_sample_time = now;

  const auto dist = read_ultrasonic(SLAVE_TRIG_PIN, SLAVE_ECHO_PIN);
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

void send_response_packet() {
  rs485_packet_t pkt;
  pkt.sync = RS485_SYNC_BYTE;
  pkt.node_id = (uint8_t)NANO_NODE_ID;
  pkt.subnode_count = 1;

  uint16_t dist = (cached_distance_cm >= 0.0f) ? (uint16_t)cached_distance_cm : 0;
  pkt.distance_cm = dist;

  uint8_t flags = STATUS_FLAG_HB;
  if (latched_motion) {
    flags |= STATUS_FLAG_MOTION;
  }
  if (latched_panic) {
    flags |= STATUS_FLAG_PANIC;
  }
  if (sensor_timeout_error) {
    flags |= STATUS_FLAG_ERROR;
  }
  pkt.status_flags = flags;

  pkt.crc = calculate_crc16((const uint8_t*)&pkt, 6);

  digitalWrite(SLAVE_RS485_DE_PIN, HIGH);
  delayMicroseconds(50);
  Serial.write((const uint8_t*)&pkt, sizeof(pkt));
  Serial.flush();
  digitalWrite(SLAVE_RS485_DE_PIN, LOW);

  latched_motion = false;
  latched_panic = false;
}

} // namespace

void setup() {
  init_ultrasonic(SLAVE_TRIG_PIN, SLAVE_ECHO_PIN);
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
