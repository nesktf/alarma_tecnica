#include "server.hpp"

#include "server_http.hpp"

#include <LittleFS.h>

#define SRL_BAUD 115200

gw_state_packet_t master_snapshot;
bool snapshot_received = false;
uint32_t last_snapshot_rx_ms = 0;

namespace {

void init_leds() {
  for (uint8_t i = 0; i < 5; i++) {
    pinMode(SECTOR_LEDS[i], OUTPUT);
    digitalWrite(SECTOR_LEDS[i], LOW);
  }
}

void init_fs() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS: Mount failed");
    while (1)
      delay(1000);
  }
  Serial.println("LittleFS: Initialized");
}

void poll_master_serial() {
  while (Serial.available() >= (int)sizeof(gw_state_packet_t)) {
    if (Serial.peek() != GW_SYNC_BYTE) {
      Serial.read();
      continue;
    }

    gw_state_packet_t pkt;
    Serial.readBytes((char*)&pkt, sizeof(pkt));

    uint16_t expected_crc = calculate_crc16((const uint8_t*)&pkt, sizeof(pkt) - 2);
    if (expected_crc == pkt.crc && pkt.msg_type == GW_MSG_STATE_SNAPSHOT) {
      memcpy(&master_snapshot, &pkt, sizeof(pkt));
      snapshot_received = true;
      last_snapshot_rx_ms = millis();
    }
  }
}

void update_sector_leds() {
  if (!snapshot_received || (millis() - last_snapshot_rx_ms > 5000)) {
    for (uint8_t i = 0; i < 5; i++) {
      digitalWrite(SECTOR_LEDS[i], LOW);
    }
    return;
  }

  for (uint8_t i = 0; i < 5; i++) {
    bool on =
      (master_snapshot.sectors[i].enabled != 0) && (master_snapshot.sectors[i].online != 0);
    digitalWrite(SECTOR_LEDS[i], on ? HIGH : LOW);
  }
}

} // namespace

void send_command_to_master(uint8_t msg_type, uint8_t param1, uint8_t param2) {
  gw_cmd_packet_t cmd;
  cmd.sync = GW_SYNC_BYTE;
  cmd.msg_type = msg_type;
  cmd.payload_len = 2;
  cmd.param1 = param1;
  cmd.param2 = param2;
  cmd.crc = calculate_crc16((const uint8_t*)&cmd, sizeof(cmd) - 2);

  Serial.write((const uint8_t*)&cmd, sizeof(cmd));
  Serial.flush();
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial && millis() < 1000)
    ;

  init_leds();
  init_fs();
  init_http_server();
}

void loop() {
  handle_server_client();
  poll_master_serial();
  update_sector_leds();
}
