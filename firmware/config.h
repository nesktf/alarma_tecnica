#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define TOTAL_SECTORS 5

enum SectorId : uint8_t {
    SEC_OPERATIVA     = 1,
    SEC_AUTOMOTORES   = 2,
    SEC_TALLER        = 3,
    SEC_VICEDIRECCION = 4,
    SEC_INFORMATICA   = 5
};

// Timing and detection thresholds
#define RS485_POLL_INTERVAL_MS           1000UL
#define RS485_TIMEOUT_MS                 100UL
#define NODE_OFFLINE_TIMEOUT_MS          300000UL
#define ARMING_EXIT_DELAY_MS             30000UL
#define CONSECUTIVE_MOTION_TRIGGER_COUNT 3
#define MOTION_DISTANCE_THRESHOLD_CM     150

#define DEFAULT_KEYPAD_PASSCODE          "1234"

// SHA-256 hash of default web password "admin123"
#define DEFAULT_WEB_PASSWORD_SHA256      "240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9"

// Master Nano pin budget
// Hardware Serial D0/D1 links to NodeMCU ESP8266 via 3.3V-5V level shifter
#define MASTER_RS485_RX_PIN              2
#define MASTER_RS485_TX_PIN              3
#define MASTER_RS485_DE_PIN              4

#define MASTER_SIREN_PIN                 5

#define MASTER_LED_GREEN_PIN             6
#define MASTER_LED_YELLOW_PIN            7
#define MASTER_LED_RED_PIN               8

#define MASTER_TRIG_PIN                  9
#define MASTER_ECHO_PIN                  10

#define MASTER_PANIC_PIN                 11

// Keypad 4x4 matrix pins (rows on D12, D13, A0, A1; cols on A2 to A5)
#define KEYPAD_ROW_1_PIN                 12
#define KEYPAD_ROW_2_PIN                 13
#define KEYPAD_ROW_3_PIN                 A0
#define KEYPAD_ROW_4_PIN                 A1

#define KEYPAD_COL_1_PIN                 A2
#define KEYPAD_COL_2_PIN                 A3
#define KEYPAD_COL_3_PIN                 A4
#define KEYPAD_COL_4_PIN                 A5

// Slave Nanos pin budget (Hardware Serial D0/D1 dedicated to RS485 bus)
#define SLAVE_RS485_DE_PIN               2
#define SLAVE_TRIG_PIN                   9
#define SLAVE_ECHO_PIN                   10
#define SLAVE_PANIC_PIN                  11

// NodeMCU ESP8266 sector status LED outputs
#define NODEMCU_LED_SEC_OPERATIVA        D1
#define NODEMCU_LED_SEC_AUTOMOTORES      D2
#define NODEMCU_LED_SEC_TALLER           D5
#define NODEMCU_LED_SEC_VICEDIRECCION    D6
#define NODEMCU_LED_SEC_INFORMATICA      D7

#endif // CONFIG_H
