#include "master_rs485.hpp"

#include "master_node.hpp"

#include <Arduino.h>
#include <SoftwareSerial.h>

namespace {

SoftwareSerial rs485_bus(MASTER_RS485_RX_PIN, MASTER_RS485_TX_PIN);
uint32_t last_poll_ms = 0;
uint8_t poll_index = 2;
uint32_t last_gw_broadcast_ms = 0;

void set_rs485_de(bool transmit) {
  digitalWrite(MASTER_RS485_DE_PIN, transmit ? HIGH : LOW);
  if (transmit) {
    delayMicroseconds(40);
  }
}

void send_gateway_snapshot() {
  const auto state = current_alarm_state();

  gw_state_packet_t snap;
  snap.sync = GW_SYNC_BYTE;
  snap.msg_type = GW_MSG_STATE_SNAPSHOT;
  snap.payload_len = sizeof(snap) - 5;

  snap.alarm_state = (uint8_t)state;
  snap.trigger_reason = (uint8_t)current_trigger_reason();
  snap.trigger_sector = current_trigger_sector();

  if (state == ALARM_STATE_ARMING) {
    uint32_t elapsed = millis() - arming_start_time();
    if (elapsed < ARMING_EXIT_DELAY_MS) {
      snap.arming_time_s = (uint16_t)((ARMING_EXIT_DELAY_MS - elapsed) / 1000);
    } else {
      snap.arming_time_s = 0;
      set_alarm_state(ALARM_STATE_ARMED);
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
    snap.sectors[i].last_seen_ms =
      (now > sectors[sec_id].last_seen_ms) ? (now - sectors[sec_id].last_seen_ms) : 0;
  }

  snap.crc = calculate_crc16((const uint8_t*)&snap, sizeof(snap) - 2);

  Serial.write((const uint8_t*)&snap, sizeof(snap));
}

void handle_gateway_rx() {
  while (Serial.available() >= (int)sizeof(gw_cmd_packet_t)) {
    if (Serial.peek() != GW_SYNC_BYTE) {
      Serial.read();
      continue;
    }

    gw_cmd_packet_t cmd;
    Serial.readBytes((char*)&cmd, sizeof(cmd));

    uint16_t expected_crc = calculate_crc16((const uint8_t*)&cmd, sizeof(cmd) - 2);
    if (expected_crc != cmd.crc)
      continue;

    switch (cmd.msg_type) {
      case GW_MSG_SECTOR_TOGGLE: {
        uint8_t sec_id = cmd.param1;
        uint8_t state = cmd.param2;
        if (sec_id >= 1 && sec_id <= TOTAL_SECTORS) {
          sectors[sec_id].enabled = (state != 0);
        } else if (sec_id == 0) {
          for (uint8_t s = 1; s <= TOTAL_SECTORS; s++) {
            sectors[s].enabled = (state != 0);
          }
        }
      } break;
      case GW_MSG_MANUAL_PING: {
        uint8_t sec_id = cmd.param1;
        if (sec_id >= 2 && sec_id <= TOTAL_SECTORS) {
          poll_index = sec_id;
          last_poll_ms = 0;
        }
      } break;
      case GW_MSG_SYSTEM_ARM: {
        uint8_t target_arm = cmd.param1;
        if (target_arm == ALARM_STATE_ARMED || target_arm == ALARM_STATE_ARMING) {
          start_arming();
        } else {
          disarm_system();
        }
      } break;
      default:
        break;
    }

    send_gateway_snapshot();
  }
}

} // namespace

void init_rs485_bus() {
  pinMode(MASTER_RS485_DE_PIN, OUTPUT);
  digitalWrite(MASTER_RS485_DE_PIN, LOW);

  rs485_bus.begin(9600);
}

void poll_rs485_slaves() {
  handle_gateway_rx();

  uint32_t now = millis();
  if (now - last_gw_broadcast_ms >= 500) {
    last_gw_broadcast_ms = now;
    send_gateway_snapshot();
  }

  if (now - last_poll_ms < RS485_POLL_INTERVAL_MS) {
    return;
  }
  last_poll_ms = now;

  uint8_t target_id = poll_index;
  poll_index++;
  if (poll_index > TOTAL_SECTORS) {
    poll_index = 2;
  }

  while (rs485_bus.available()) {
    rs485_bus.read();
  }

  set_rs485_de(true);
  rs485_bus.write(RS485_POLL_CMD_PREFIX | target_id);
  set_rs485_de(false);

  uint32_t start_wait = millis();
  rs485_packet_t pkt;
  uint8_t bytes_read = 0;
  uint8_t* raw_buf = (uint8_t*)&pkt;

  while (millis() - start_wait < RS485_TIMEOUT_MS) {
    while (rs485_bus.available() && bytes_read < sizeof(rs485_packet_t)) {
      raw_buf[bytes_read++] = (uint8_t)rs485_bus.read();
    }
    if (bytes_read == sizeof(rs485_packet_t)) {
      break;
    }
  }

  if (bytes_read == sizeof(pkt) && pkt.sync == RS485_SYNC_BYTE && pkt.node_id == target_id) {
    const uint16_t expected_crc = calculate_crc16(raw_buf, 6);
    if (expected_crc == pkt.crc) {
      sectors[target_id].online = true;
      sectors[target_id].last_seen_ms = now;
      sectors[target_id].distance_cm = pkt.distance_cm;

      const bool motion = (pkt.status_flags & STATUS_FLAG_MOTION) != 0;
      const bool panic = (pkt.status_flags & STATUS_FLAG_PANIC) != 0;
      sectors[target_id].motion = motion;
      sectors[target_id].panic = panic;

      if (panic) {
        trigger_alarm(TRIGGER_PANIC, target_id);
      }

      const auto alarm_state = current_alarm_state();
      if (sectors[target_id].enabled && alarm_state == ALARM_STATE_ARMED) {
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

  const auto alarm_state = current_alarm_state();
  for (uint8_t i = 2; i <= TOTAL_SECTORS; i++) {
    if (now - sectors[i].last_seen_ms > NODE_OFFLINE_TIMEOUT_MS) {
      sectors[i].online = false;
      if (sectors[i].enabled && alarm_state == ALARM_STATE_ARMED) {
        trigger_alarm(TRIGGER_DISCONNECT, i);
      }
    }
  }
}
