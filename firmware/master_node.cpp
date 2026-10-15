#include <Arduino.h>
#include <SoftwareSerial.h>
#include "config.h"
#include "protocol.h"

static SoftwareSerial rs485_bus(MASTER_RS485_RX_PIN, MASTER_RS485_TX_PIN);

struct SectorRuntime {
    uint8_t  id;
    bool     enabled;
    bool     online;
    bool     motion;
    bool     panic;
    uint16_t distance_cm;
    uint32_t last_seen_ms;
    uint8_t  consecutive_motions;
};

static SectorRuntime sectors[TOTAL_SECTORS + 1];

static AlarmState current_alarm_state = ALARM_STATE_DISARMED;
static TriggerReason current_trigger_reason = TRIGGER_NONE;
static uint8_t current_trigger_sector = 0;
static uint32_t arming_start_ms = 0;

static char keypad_buffer[16];
static uint8_t keypad_buffer_len = 0;
static uint32_t last_keypress_ms = 0;

static uint32_t led_feedback_until_ms = 0;
static bool led_feedback_success = false;

static uint32_t last_poll_ms = 0;
static uint8_t  poll_index = 2;
static uint32_t last_gw_broadcast_ms = 0;
static uint32_t last_local_sensor_ms = 0;

static void set_rs485_de(bool transmit) {
    digitalWrite(MASTER_RS485_DE_PIN, transmit ? HIGH : LOW);
    if (transmit) {
        delayMicroseconds(40);
    }
}

static const char KEYMAP[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static const uint8_t ROW_PINS[4] = {
    KEYPAD_ROW_1_PIN, KEYPAD_ROW_2_PIN, KEYPAD_ROW_3_PIN, KEYPAD_ROW_4_PIN
};

static const uint8_t COL_PINS[4] = {
    KEYPAD_COL_1_PIN, KEYPAD_COL_2_PIN, KEYPAD_COL_3_PIN, KEYPAD_COL_4_PIN
};

static void init_keypad() {
    for (uint8_t r = 0; r < 4; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    for (uint8_t c = 0; c < 4; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }
}

static char scan_keypad() {
    static char last_key = '\0';
    static uint32_t last_scan_time = 0;

    if (millis() - last_scan_time < 50) return '\0';
    last_scan_time = millis();

    char pressed_key = '\0';

    for (uint8_t r = 0; r < 4; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        for (uint8_t c = 0; c < 4; c++) {
            if (digitalRead(COL_PINS[c]) == LOW) {
                pressed_key = KEYMAP[r][c];
                break;
            }
        }
        digitalWrite(ROW_PINS[r], HIGH);
        if (pressed_key != '\0') break;
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

static void trigger_alarm(TriggerReason reason, uint8_t sector_id) {
    current_alarm_state = ALARM_STATE_ALARM;
    current_trigger_reason = reason;
    current_trigger_sector = sector_id;
    digitalWrite(MASTER_SIREN_PIN, HIGH);
}

static void disarm_system() {
    current_alarm_state = ALARM_STATE_DISARMED;
    current_trigger_reason = TRIGGER_NONE;
    current_trigger_sector = 0;
    digitalWrite(MASTER_SIREN_PIN, LOW);
    for (uint8_t i = 1; i <= TOTAL_SECTORS; i++) {
        sectors[i].consecutive_motions = 0;
        sectors[i].motion = false;
        sectors[i].panic = false;
    }
}

static void start_arming() {
    if (current_alarm_state == ALARM_STATE_DISARMED) {
        current_alarm_state = ALARM_STATE_ARMING;
        arming_start_ms = millis();
        for (uint8_t i = 1; i <= TOTAL_SECTORS; i++) {
            sectors[i].consecutive_motions = 0;
        }
    }
}

static void execute_keypad_action(char func_key) {
    keypad_buffer[keypad_buffer_len] = '\0';
    bool pass_valid = (strcmp(keypad_buffer, DEFAULT_KEYPAD_PASSCODE) == 0);

    keypad_buffer_len = 0;

    if (!pass_valid) {
        led_feedback_until_ms = millis() + 600;
        led_feedback_success = false;
        return;
    }

    led_feedback_until_ms = millis() + 400;
    led_feedback_success = true;

    switch (func_key) {
        case '*':
            if (current_alarm_state == ALARM_STATE_ALARM ||
                current_alarm_state == ALARM_STATE_ARMED ||
                current_alarm_state == ALARM_STATE_ARMING) {
                disarm_system();
            } else {
                start_arming();
            }
            break;

        case '#':
            sectors[SEC_OPERATIVA].enabled = !sectors[SEC_OPERATIVA].enabled;
            break;

        case 'A':
            sectors[SEC_AUTOMOTORES].enabled = !sectors[SEC_AUTOMOTORES].enabled;
            break;

        case 'B':
            sectors[SEC_VICEDIRECCION].enabled = !sectors[SEC_VICEDIRECCION].enabled;
            break;

        case 'C':
            sectors[SEC_INFORMATICA].enabled = !sectors[SEC_INFORMATICA].enabled;
            break;

        case 'D':
            sectors[SEC_TALLER].enabled = !sectors[SEC_TALLER].enabled;
            break;

        default:
            break;
    }
}

static void handle_keypad() {
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

static float read_local_ultrasonic() {
    digitalWrite(MASTER_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(MASTER_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(MASTER_TRIG_PIN, LOW);

    unsigned long duration = pulseIn(MASTER_ECHO_PIN, HIGH, 30000UL);
    if (!duration) return -1.0f;
    return (float)duration * 0.034f / 2.0f;
}

static void poll_local_area() {
    uint32_t now = millis();
    if (now - last_local_sensor_ms < 100) return;
    last_local_sensor_ms = now;

    // Panic button has 24/7 immediate priority
    bool panic_now = (digitalRead(MASTER_PANIC_PIN) == LOW);
    if (panic_now) {
        sectors[SEC_OPERATIVA].panic = true;
        trigger_alarm(TRIGGER_PANIC, SEC_OPERATIVA);
    }

    float dist = read_local_ultrasonic();
    sectors[SEC_OPERATIVA].online = true;
    sectors[SEC_OPERATIVA].last_seen_ms = now;

    if (dist >= 0.0f) {
        sectors[SEC_OPERATIVA].distance_cm = (uint16_t)dist;
        bool motion = (dist < (float)MOTION_DISTANCE_THRESHOLD_CM);
        sectors[SEC_OPERATIVA].motion = motion;

        if (sectors[SEC_OPERATIVA].enabled && current_alarm_state == ALARM_STATE_ARMED) {
            if (motion) {
                sectors[SEC_OPERATIVA].consecutive_motions++;
                if (sectors[SEC_OPERATIVA].consecutive_motions >= CONSECUTIVE_MOTION_TRIGGER_COUNT) {
                    trigger_alarm(TRIGGER_MOTION, SEC_OPERATIVA);
                }
            } else {
                sectors[SEC_OPERATIVA].consecutive_motions = 0;
            }
        }
    }
}

static void poll_rs485_slaves() {
    uint32_t now = millis();
    if (now - last_poll_ms < RS485_POLL_INTERVAL_MS) return;
    last_poll_ms = now;

    uint8_t target_id = poll_index;
    poll_index++;
    if (poll_index > TOTAL_SECTORS) poll_index = 2;

    while (rs485_bus.available()) rs485_bus.read();

    set_rs485_de(true);
    rs485_bus.write(RS485_POLL_CMD_PREFIX | target_id);
    set_rs485_de(false);

    uint32_t start_wait = millis();
    rs485_packet_t pkt;
    uint8_t bytes_read = 0;
    uint8_t *raw_buf = (uint8_t *)&pkt;

    while (millis() - start_wait < RS485_TIMEOUT_MS) {
        while (rs485_bus.available() && bytes_read < sizeof(rs485_packet_t)) {
            raw_buf[bytes_read++] = (uint8_t)rs485_bus.read();
        }
        if (bytes_read == sizeof(rs485_packet_t)) break;
    }

    if (bytes_read == sizeof(rs485_packet_t) && pkt.sync == RS485_SYNC_BYTE && pkt.node_id == target_id) {
        uint16_t expected_crc = calculate_crc16(raw_buf, 6);
        if (expected_crc == pkt.crc) {
            sectors[target_id].online = true;
            sectors[target_id].last_seen_ms = now;
            sectors[target_id].distance_cm = pkt.distance_cm;

            bool motion = (pkt.status_flags & STATUS_FLAG_MOTION) != 0;
            bool panic  = (pkt.status_flags & STATUS_FLAG_PANIC) != 0;

            sectors[target_id].motion = motion;
            sectors[target_id].panic = panic;

            if (panic) {
                trigger_alarm(TRIGGER_PANIC, target_id);
            }

            if (sectors[target_id].enabled && current_alarm_state == ALARM_STATE_ARMED) {
                if (motion) {
                    sectors[target_id].consecutive_motions++;
                    if (sectors[target_id].consecutive_motions >= CONSECUTIVE_MOTION_TRIGGER_COUNT) {
                        trigger_alarm(TRIGGER_MOTION, target_id);
                    }
                } else {
                    sectors[target_id].consecutive_motions = 0;
                }
            }
        }
    }

    for (uint8_t i = 2; i <= TOTAL_SECTORS; i++) {
        if (now - sectors[i].last_seen_ms > NODE_OFFLINE_TIMEOUT_MS) {
            sectors[i].online = false;
            if (sectors[i].enabled && current_alarm_state == ALARM_STATE_ARMED) {
                trigger_alarm(TRIGGER_DISCONNECT, i);
            }
        }
    }
}

static void send_gateway_snapshot() {
    gw_state_packet_t snap;
    snap.sync = GW_SYNC_BYTE;
    snap.msg_type = GW_MSG_STATE_SNAPSHOT;
    snap.payload_len = sizeof(snap) - 5;

    snap.alarm_state = (uint8_t)current_alarm_state;
    snap.trigger_reason = (uint8_t)current_trigger_reason;
    snap.trigger_sector = current_trigger_sector;

    if (current_alarm_state == ALARM_STATE_ARMING) {
        uint32_t elapsed = millis() - arming_start_ms;
        if (elapsed < ARMING_EXIT_DELAY_MS) {
            snap.arming_time_s = (uint16_t)((ARMING_EXIT_DELAY_MS - elapsed) / 1000);
        } else {
            snap.arming_time_s = 0;
            current_alarm_state = ALARM_STATE_ARMED;
        }
    } else {
        snap.arming_time_s = 0;
    }

    uint32_t now = millis();
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t sec_id = i + 1;
        snap.sectors[i].id = sec_id;
        snap.sectors[i].enabled = sectors[sec_id].enabled ? 1 : 0;
        snap.sectors[i].online = sectors[sec_id].online ? 1 : 0;
        snap.sectors[i].motion = sectors[sec_id].motion ? 1 : 0;
        snap.sectors[i].panic = sectors[sec_id].panic ? 1 : 0;
        snap.sectors[i].distance_cm = sectors[sec_id].distance_cm;
        snap.sectors[i].last_seen_ms = (now > sectors[sec_id].last_seen_ms) ? (now - sectors[sec_id].last_seen_ms) : 0;
    }

    snap.crc = calculate_crc16((const uint8_t *)&snap, sizeof(snap) - 2);

    Serial.write((const uint8_t *)&snap, sizeof(snap));
}

static void handle_gateway_rx() {
    while (Serial.available() >= (int)sizeof(gw_cmd_packet_t)) {
        if (Serial.peek() != GW_SYNC_BYTE) {
            Serial.read();
            continue;
        }

        gw_cmd_packet_t cmd;
        Serial.readBytes((char *)&cmd, sizeof(cmd));

        uint16_t expected_crc = calculate_crc16((const uint8_t *)&cmd, sizeof(cmd) - 2);
        if (expected_crc != cmd.crc) continue;

        switch (cmd.msg_type) {
            case GW_MSG_SECTOR_TOGGLE: {
                uint8_t sec_id = cmd.param1;
                uint8_t state  = cmd.param2;
                if (sec_id >= 1 && sec_id <= TOTAL_SECTORS) {
                    sectors[sec_id].enabled = (state != 0);
                } else if (sec_id == 0) {
                    for (uint8_t s = 1; s <= TOTAL_SECTORS; s++) {
                        sectors[s].enabled = (state != 0);
                    }
                }
                break;
            }
            case GW_MSG_MANUAL_PING: {
                uint8_t sec_id = cmd.param1;
                if (sec_id >= 2 && sec_id <= TOTAL_SECTORS) {
                    poll_index = sec_id;
                    last_poll_ms = 0;
                }
                break;
            }
            case GW_MSG_SYSTEM_ARM: {
                uint8_t target_arm = cmd.param1;
                if (target_arm == ALARM_STATE_ARMED || target_arm == ALARM_STATE_ARMING) {
                    start_arming();
                } else {
                    disarm_system();
                }
                break;
            }
            default:
                break;
        }

        send_gateway_snapshot();
    }
}

static void update_leds() {
    uint32_t now = millis();

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

    switch (current_alarm_state) {
        case ALARM_STATE_DISARMED:
            digitalWrite(MASTER_LED_GREEN_PIN, HIGH);
            digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
            digitalWrite(MASTER_LED_RED_PIN, LOW);
            break;

        case ALARM_STATE_ARMING:
            digitalWrite(MASTER_LED_GREEN_PIN, LOW);
            digitalWrite(MASTER_LED_YELLOW_PIN, ((now / 250) % 2 == 0) ? HIGH : LOW);
            digitalWrite(MASTER_LED_RED_PIN, LOW);
            break;

        case ALARM_STATE_ARMED:
            digitalWrite(MASTER_LED_GREEN_PIN, LOW);
            digitalWrite(MASTER_LED_YELLOW_PIN, ((now % 2000) < 150) ? HIGH : LOW);
            digitalWrite(MASTER_LED_RED_PIN, LOW);
            break;

        case ALARM_STATE_ALARM:
            digitalWrite(MASTER_LED_GREEN_PIN, LOW);
            digitalWrite(MASTER_LED_YELLOW_PIN, LOW);
            digitalWrite(MASTER_LED_RED_PIN, HIGH);
            break;
    }
}

void setup() {
    pinMode(MASTER_SIREN_PIN, OUTPUT);
    digitalWrite(MASTER_SIREN_PIN, LOW);

    pinMode(MASTER_LED_GREEN_PIN, OUTPUT);
    pinMode(MASTER_LED_YELLOW_PIN, OUTPUT);
    pinMode(MASTER_LED_RED_PIN, OUTPUT);

    pinMode(MASTER_RS485_DE_PIN, OUTPUT);
    digitalWrite(MASTER_RS485_DE_PIN, LOW);

    pinMode(MASTER_TRIG_PIN, OUTPUT);
    pinMode(MASTER_ECHO_PIN, INPUT);
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
    rs485_bus.begin(9600);
}

void loop() {
    handle_keypad();
    poll_local_area();
    poll_rs485_slaves();
    handle_gateway_rx();

    if (millis() - last_gw_broadcast_ms >= 500) {
        last_gw_broadcast_ms = millis();
        send_gateway_snapshot();
    }

    update_leds();
}
