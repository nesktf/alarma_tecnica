#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// RS485 field bus framing (half-duplex)
#define RS485_SYNC_BYTE       0xAA
#define RS485_POLL_CMD_PREFIX 0xB0

#define STATUS_FLAG_MOTION 0x01
#define STATUS_FLAG_ERROR  0x02
#define STATUS_FLAG_PANIC  0x04
#define STATUS_FLAG_HB     0x08

typedef struct __attribute__((packed)) {
  uint8_t sync;
  uint8_t node_id;
  uint8_t subnode_count;
  uint16_t distance_cm;
  uint8_t status_flags;
  uint16_t crc;
} rs485_packet_t;

// Gateway serial link (Master Nano <-> NodeMCU ESP8266)
#define GW_SYNC_BYTE 0x55

enum GwMsgType : uint8_t {
  GW_MSG_STATE_SNAPSHOT = 0x01,
  GW_MSG_SECTOR_TOGGLE = 0x02,
  GW_MSG_MANUAL_PING = 0x03,
  GW_MSG_SYSTEM_ARM = 0x04,
  GW_MSG_ACK_NACK = 0x05
};

enum AlarmState : uint8_t {
  ALARM_STATE_DISARMED = 0x00,
  ALARM_STATE_ARMING = 0x01,
  ALARM_STATE_ARMED = 0x02,
  ALARM_STATE_ALARM = 0x03,
  ALARM_STATE_GRACE = 0x04
};

enum TriggerReason : uint8_t {
  TRIGGER_NONE = 0x00,
  TRIGGER_PANIC = 0x01,
  TRIGGER_MOTION = 0x02,
  TRIGGER_DISCONNECT = 0x03
};

typedef struct __attribute__((packed)) {
  uint8_t id;
  uint8_t enabled;
  uint8_t online;
  uint8_t motion;
  uint8_t panic;
  uint16_t distance_cm;
  uint32_t last_seen_ms;
} gw_sector_state_t;

typedef struct __attribute__((packed)) {
  uint8_t sync;
  uint8_t msg_type;
  uint8_t payload_len;
  uint8_t alarm_state;
  uint16_t arming_time_s;
  uint16_t grace_time_s;
  uint8_t trigger_reason;
  uint8_t trigger_sector;
  gw_sector_state_t sectors[5];
  uint16_t crc;
} gw_state_packet_t;

typedef struct __attribute__((packed)) {
  uint8_t sync;
  uint8_t msg_type;
  uint8_t payload_len;
  uint8_t param1;
  uint8_t param2;
  uint16_t crc;
} gw_cmd_packet_t;

// CRC-16-CCITT (XModem, polynomial 0x1021)
static inline uint16_t calculate_crc16(const uint8_t* data, uint16_t len) {
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

#endif // PROTOCOL_H
