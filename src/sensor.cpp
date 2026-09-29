#include "sensor.hpp"
#include <ESP8266WiFi.h>

#define TRIG_PIN D6
#define ECHO_PIN D5

#define SOUND_VEL 0.034

void init_sensor() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

bool filesystem_ready = false;
static bool measurement_valid = false;
static float last_distance_cm = 0;
static unsigned long last_measurement = 0;

static bool read_sensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);

  const auto duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (!duration) {
    return false;
  }
  last_distance_cm = static_cast<float>(duration) * SOUND_VEL / 2.0f;
  return true;
}

void poll_sensor() {
  if (millis() - last_measurement >= 1000) {
    last_measurement = millis();
    measurement_valid = read_sensor();
    if (measurement_valid) {
      Serial.print("Distance: ");
      Serial.println(last_distance_cm);
    } else {
      Serial.println("Distance: no echo");
    }
  }
}

int format_sensor_state(char* buff, size_t sz) {
  char distance[16];
  if (measurement_valid) {
    snprintf(distance, sizeof(distance), "%.1f", last_distance_cm);
  } else {
    snprintf(distance, sizeof(distance), "null");
  }

  return snprintf(
    buff,
    sz,
    "{\"simulator\":false,\"board\":\"NodeMCU ESP8266\",\"state\":\"running\","
    "\"wifi\":true,\"filesystem\":%s,\"uptime_ms\":%lu,"
    "\"sensor\":{\"valid\":%s,\"distance_cm\":%s,"
    "\"age_ms\":%ld,\"echo_timeout_us\":30000},"
    "\"pins\":{\"trigger_gpio\":%u,\"echo_gpio\":%u,\"trigger_label\":\"D6\",\"echo_label\":\"D5\"},"
    "\"limits\":{\"cpu_mhz\":80,\"flash_bytes\":4194304,\"dram_bytes\":80192,\"iram_bytes\":65536}}",
    filesystem_ready ? "true" : "false",
    (unsigned long)millis(),
    measurement_valid ? "true" : "false",
    distance,
    measurement_valid ? (long)(millis() - last_measurement) : -1L,
    (unsigned)TRIG_PIN,
    (unsigned)ECHO_PIN
  );
}
