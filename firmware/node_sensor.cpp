#include "node_sensor.hpp"

#include <Arduino.h>

void init_ultrasonic(uint8_t trig_pin, uint8_t echo_pin) {
  pinMode(trig_pin, OUTPUT);
  pinMode(echo_pin, INPUT);
}

float read_ultrasonic(uint8_t trig_pin, uint8_t echo_pin) {
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);

  unsigned long duration = pulseIn(echo_pin, HIGH, 30000UL);
  if (!duration) {
    return -1.0f;
  }
  return (float)duration * 0.034f / 2.0f;
}
