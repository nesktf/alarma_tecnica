#include "sensor.hpp"
#include <ESP8266WiFi.h>

#define TRIG_PIN 22
#define ECHO_PIN 21

#define SOUND_VEL 0.034

void init_led() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void blink_led() {
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
}

void init_sensor() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

float read_sensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);

  const auto duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  return static_cast<float>(duration) * SOUND_VEL / 2.0f;
}

static unsigned long last_measurement = 0;

void print_sensor() {
  if (millis() - last_measurement >= 1000) {
    last_measurement = millis();
    const auto d = read_sensor();
    Serial.print("Distance: ");
    Serial.println(d);
  }
}
