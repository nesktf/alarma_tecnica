#include "master_node.hpp"

#include "master_keypad.hpp"
#include "master_rs485.hpp"
#include "node_sensor.hpp"

#include <Arduino.h>
#include <SoftwareSerial.h>

SectorRuntime sectors[TOTAL_SECTORS + 1];
uint32_t led_feedback_until_ms = 0;
bool led_feedback_success = false;

namespace {

uint32_t arming_start_ms = 0;
uint32_t grace_start_ms = 0;
uint32_t last_local_sensor_ms = 0;
TriggerReason trigger_reason = TRIGGER_NONE;
uint8_t trigger_sector = 0;
AlarmState alarm_state = ALARM_STATE_DISARMED;

void poll_local_area() {
  uint32_t now = millis();
  if (now - last_local_sensor_ms < 100) {
    return;
  }
  last_local_sensor_ms = now;

  // Panic button has 24/7 immediate priority
  const bool panic_now = (digitalRead(MASTER_PANIC_PIN) == LOW);
  if (panic_now) {
    sectors[SEC_OPERATIVA].panic = true;
    trigger_alarm(TRIGGER_PANIC, SEC_OPERATIVA);
  }

  const auto dist = read_ultrasonic(MASTER_TRIG_PIN, MASTER_ECHO_PIN);
  sectors[SEC_OPERATIVA].online = true;
  sectors[SEC_OPERATIVA].last_seen_ms = now;

  if (dist < 0.0f) {
    return;
  }

  const bool motion = (dist < (float)MOTION_DISTANCE_THRESHOLD_CM);
  sectors[SEC_OPERATIVA].distance_cm = (uint16_t)dist;
  sectors[SEC_OPERATIVA].motion = motion;

  if (sectors[SEC_OPERATIVA].enabled && alarm_state == ALARM_STATE_ARMED) {
    if (motion) {
      sectors[SEC_OPERATIVA].consecutive_motions++;
      if (sectors[SEC_OPERATIVA].consecutive_motions >= CONSECUTIVE_MOTION_TRIGGER_COUNT) {
        trigger_grace_period(TRIGGER_MOTION, SEC_OPERATIVA);
      }
    } else {
      sectors[SEC_OPERATIVA].consecutive_motions = 0;
    }
  }
}

void update_alarm_timers() {
  const uint32_t now = millis();
  if (alarm_state == ALARM_STATE_ARMING) {
    if (now - arming_start_ms >= ARMING_EXIT_DELAY_MS) {
      alarm_state = ALARM_STATE_ARMED;
    }
  } else if (alarm_state == ALARM_STATE_GRACE) {
    if (now - grace_start_ms >= ALARM_GRACE_PERIOD_MS) {
      trigger_alarm(trigger_reason, trigger_sector);
    }
  }
}

void update_leds() {
  const uint32_t now = millis();

  if (now < led_feedback_until_ms) {
    if (led_feedback_success) {
      digitalWrite(MASTER_LED_GREEN_PIN, ((now / 100) % 2 == 0) ? HIGH : LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
      digitalWrite(MASTER_LED_RED_PIN, LOW);
    } else {
      digitalWrite(MASTER_LED_GREEN_PIN, LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
      digitalWrite(MASTER_LED_RED_PIN, ((now / 100) % 2 == 0) ? HIGH : LOW);
    }
    return;
  }

  switch (alarm_state) {
    case ALARM_STATE_DISARMED: {
      digitalWrite(MASTER_LED_GREEN_PIN, HIGH);
      digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
      digitalWrite(MASTER_LED_RED_PIN, LOW);
    } break;
    case ALARM_STATE_ARMING: {
      digitalWrite(MASTER_LED_GREEN_PIN, LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, ((now / 250) % 2 == 0) ? HIGH : LOW);
      digitalWrite(MASTER_LED_RED_PIN, LOW);
    } break;
    case ALARM_STATE_ARMED: {
      digitalWrite(MASTER_LED_GREEN_PIN, LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, ((now % 2000) < 150) ? HIGH : LOW);
      digitalWrite(MASTER_LED_RED_PIN, LOW);
    } break;
    case ALARM_STATE_GRACE: {
      digitalWrite(MASTER_LED_GREEN_PIN, LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
      digitalWrite(MASTER_LED_RED_PIN, ((now / 250) % 2 == 0) ? HIGH : LOW);
    } break;
    case ALARM_STATE_ALARM: {
      digitalWrite(MASTER_LED_GREEN_PIN, LOW);
      digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
      digitalWrite(MASTER_LED_RED_PIN, HIGH);
    } break;
  }
}

} // namespace

void trigger_grace_period(TriggerReason reason, uint8_t sector_id) {
  if (alarm_state != ALARM_STATE_ARMED) {
    return;
  }
  alarm_state = ALARM_STATE_GRACE;
  grace_start_ms = millis();
  trigger_reason = reason;
  trigger_sector = sector_id;
  digitalWrite(MASTER_SIREN_PIN, LOW);
}

void trigger_alarm(TriggerReason reason, uint8_t sector_id) {
  alarm_state = ALARM_STATE_ALARM;
  trigger_reason = reason;
  trigger_sector = sector_id;
  digitalWrite(MASTER_SIREN_PIN, HIGH);
}

void disarm_system() {
  alarm_state = ALARM_STATE_DISARMED;
  trigger_reason = TRIGGER_NONE;
  trigger_sector = 0;
  arming_start_ms = 0;
  grace_start_ms = 0;
  digitalWrite(MASTER_SIREN_PIN, LOW);
  for (uint8_t i = 1; i <= TOTAL_SECTORS; i++) {
    sectors[i].consecutive_motions = 0;
    sectors[i].motion = false;
    sectors[i].panic = false;
  }
}

void start_arming() {
  if (alarm_state != ALARM_STATE_DISARMED) {
    return;
  }
  alarm_state = ALARM_STATE_ARMING;
  arming_start_ms = millis();
  for (uint8_t i = 1; i <= TOTAL_SECTORS; i++) {
    sectors[i].consecutive_motions = 0;
  }
}

AlarmState current_alarm_state() {
  return alarm_state;
}

TriggerReason current_trigger_reason() {
  return trigger_reason;
}

uint8_t current_trigger_sector() {
  return trigger_sector;
}

uint32_t arming_start_time() {
  return arming_start_ms;
}

uint32_t grace_start_time() {
  return grace_start_ms;
}

void set_alarm_state(AlarmState state) {
  alarm_state = state;
}

void setup() {
  pinMode(MASTER_SIREN_PIN, OUTPUT);
  digitalWrite(MASTER_SIREN_PIN, LOW);

  pinMode(MASTER_LED_GREEN_PIN, OUTPUT);
  pinMode(MASTER_LED_YELLOW_PIN, OUTPUT);
  pinMode(MASTER_LED_RED_PIN, OUTPUT);

  init_ultrasonic(MASTER_TRIG_PIN, MASTER_ECHO_PIN);
  pinMode(MASTER_PANIC_PIN, INPUT_PULLUP);

  init_keypad();

  for (uint8_t i = 1; i <= TOTAL_SECTORS; i++) {
    sectors[i].id = i;
    sectors[i].enabled = true;
    sectors[i].online = false;
    sectors[i].motion = false;
    sectors[i].panic = false;
    sectors[i].distance_cm = 0;
    sectors[i].last_seen_ms = 0;
    sectors[i].consecutive_motions = 0;
  }
  sectors[SEC_OPERATIVA].online = true;

  Serial.begin(115200);
  init_rs485_bus();
}

void loop() {
  handle_keypad();
  poll_local_area();
  poll_rs485_slaves();
  update_alarm_timers();
  update_leds();
}
