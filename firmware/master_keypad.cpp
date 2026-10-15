#include "master_keypad.hpp"

#include "master_node.hpp"

#include "credentials.h"

#include <Arduino.h>
#include <stdint.h>

#if !defined(MASTER_KEYPASS_PASSCODE)
#error "Keypass pascode not defined"
#endif

namespace {

constexpr char KEYMAP[4][4] = {{'1', '2', '3', 'A'},
                               {'4', '5', '6', 'B'},
                               {'7', '8', '9', 'C'},
                               {'*', '0', '#', 'D'}};

constexpr uint8_t ROW_PINS[4] = {KEYPAD_ROW_1_PIN, KEYPAD_ROW_2_PIN, KEYPAD_ROW_3_PIN,
                                 KEYPAD_ROW_4_PIN};

constexpr uint8_t COL_PINS[4] = {KEYPAD_COL_1_PIN, KEYPAD_COL_2_PIN, KEYPAD_COL_3_PIN,
                                 KEYPAD_COL_4_PIN};

char keypad_buffer[16];
uint8_t keypad_buffer_len = 0;
uint32_t last_keypress_ms = 0;

char scan_keypad() {
  static char last_key = '\0';
  static uint32_t last_scan_time = 0;

  if (millis() - last_scan_time < 50)
    return '\0';
  last_scan_time = millis();

  char pressed_key = '\0';

  for (uint8_t r = 0; r < 4; r++) {
    digitalWrite(ROW_PINS[r], LOW);
    delayMicroseconds(5);
    for (uint8_t c = 0; c < 4; c++) {
      if (digitalRead(COL_PINS[c]) == LOW) {
        pressed_key = KEYMAP[r][c];
        break;
      }
    }
    digitalWrite(ROW_PINS[r], HIGH);
    if (pressed_key != '\0')
      break;
  }

  if (pressed_key != '\0' && pressed_key != last_key) {
    last_key = pressed_key;
    return pressed_key;
  }
  if (pressed_key == '\0') {
    last_key = '\0';
  }
  return '\0';
}

void execute_keypad_action(char func_key) {
  keypad_buffer[keypad_buffer_len] = '\0';
  keypad_buffer_len = 0;

  const bool pass_valid = (strcmp(keypad_buffer, MASTER_KEYPASS_PASSCODE) == 0);
  if (!pass_valid) {
    led_feedback_until_ms = millis() + 600;
    led_feedback_success = false;
    return;
  }

  led_feedback_until_ms = millis() + 400;
  led_feedback_success = true;

  const auto state = current_alarm_state();
  switch (func_key) {
    case '*': {
      if (state == ALARM_STATE_ALARM || state == ALARM_STATE_ARMED ||
          state == ALARM_STATE_ARMING) {
        disarm_system();
      } else {
        start_arming();
      }
    } break;
    case '#': {
      sectors[SEC_OPERATIVA].enabled = !sectors[SEC_OPERATIVA].enabled;
    } break;
    case 'A': {
      sectors[SEC_AUTOMOTORES].enabled = !sectors[SEC_AUTOMOTORES].enabled;
    } break;
    case 'B': {
      sectors[SEC_TALLER].enabled = !sectors[SEC_TALLER].enabled;
    } break;
    case 'C': {
      sectors[SEC_VICEDIRECCION].enabled = !sectors[SEC_VICEDIRECCION].enabled;
    } break;
    case 'D': {
      sectors[SEC_INFORMATICA].enabled = !sectors[SEC_INFORMATICA].enabled;
    } break;
    default:
      break;
  }
}

} // namespace

void init_keypad() {
  for (uint8_t r = 0; r < 4; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], HIGH);
  }
  for (uint8_t c = 0; c < 4; c++) {
    pinMode(COL_PINS[c], INPUT_PULLUP);
  }
}

void handle_keypad() {
  char key = scan_keypad();
  if (key == '\0') {
    if (keypad_buffer_len > 0 && (millis() - last_keypress_ms > 5000)) {
      keypad_buffer_len = 0;
    }
    return;
  }

  last_keypress_ms = millis();

  if (key >= '0' && key <= '9') {
    if (keypad_buffer_len < sizeof(keypad_buffer) - 1) {
      keypad_buffer[keypad_buffer_len++] = key;
    }
  } else {
    execute_keypad_action(key);
  }
}
