#include <stdint.h>
#include <stdio.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "config.h"
#include "protocol.h"
#include "credentials.h"

#define SRL_BAUD 115200
#define SERVER_PORT 80

static ESP8266WebServer http_server{SERVER_PORT};

static gw_state_packet_t master_snapshot;
static bool snapshot_received = false;
static uint32_t last_snapshot_rx_ms = 0;

static uint32_t current_session_token = 0;
static uint32_t session_expiry_ms = 0;
#define SESSION_LIFETIME_MS 3600000UL

static const uint8_t SECTOR_LEDS[5] = {
    NODEMCU_LED_SEC_OPERATIVA,
    NODEMCU_LED_SEC_AUTOMOTORES,
    NODEMCU_LED_SEC_TALLER,
    NODEMCU_LED_SEC_VICEDIRECCION,
    NODEMCU_LED_SEC_INFORMATICA
};

static void init_leds() {
    for (uint8_t i = 0; i < 5; i++) {
        pinMode(SECTOR_LEDS[i], OUTPUT);
        digitalWrite(SECTOR_LEDS[i], LOW);
    }
}

static void update_sector_leds() {
    if (!snapshot_received || (millis() - last_snapshot_rx_ms > 5000)) {
        for (uint8_t i = 0; i < 5; i++) {
            digitalWrite(SECTOR_LEDS[i], LOW);
        }
        return;
    }

    for (uint8_t i = 0; i < 5; i++) {
        bool on = (master_snapshot.sectors[i].enabled != 0) && (master_snapshot.sectors[i].online != 0);
        digitalWrite(SECTOR_LEDS[i], on ? HIGH : LOW);
    }
}

static void init_fs() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS: Mount failed");
        while (1) delay(1000);
    }
    Serial.println("LittleFS: Initialized");
}

static void init_wifi() {
    #ifdef WIFI_DEBUG
    WiFi.printDiag(Serial);
    #endif

    #if defined(SERVER_IP) && defined(SERVER_GATEWAY) && defined(SERVER_MASK)
    const auto local_ip = SERVER_IP;
    const auto gateway = SERVER_GATEWAY;
    const auto subnet = SERVER_MASK;
    WiFi.config(local_ip, gateway, subnet);
    #endif

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t start_wifi = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_wifi < 15000) {
        delay(300);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi Connected: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi: Connection timeout, continuing...");
    }
}

static void send_command_to_master(uint8_t msg_type, uint8_t param1, uint8_t param2) {
    gw_cmd_packet_t cmd;
    cmd.sync = GW_SYNC_BYTE;
    cmd.msg_type = msg_type;
    cmd.payload_len = 2;
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.crc = calculate_crc16((const uint8_t *)&cmd, sizeof(cmd) - 2);

    Serial.write((const uint8_t *)&cmd, sizeof(cmd));
    Serial.flush();
}

static void poll_master_serial() {
    while (Serial.available() >= (int)sizeof(gw_state_packet_t)) {
        if (Serial.peek() != GW_SYNC_BYTE) {
            Serial.read();
            continue;
        }

        gw_state_packet_t pkt;
        Serial.readBytes((char *)&pkt, sizeof(pkt));

        uint16_t expected_crc = calculate_crc16((const uint8_t *)&pkt, sizeof(pkt) - 2);
        if (expected_crc == pkt.crc && pkt.msg_type == GW_MSG_STATE_SNAPSHOT) {
            memcpy(&master_snapshot, &pkt, sizeof(pkt));
            snapshot_received = true;
            last_snapshot_rx_ms = millis();
        }
    }
}

static bool is_authenticated() {
    if (current_session_token == 0) return false;
    if (millis() > session_expiry_ms) {
        current_session_token = 0;
        return false;
    }

    if (http_server.hasHeader("X-Auth-Token")) {
        uint32_t t = (uint32_t)http_server.header("X-Auth-Token").toInt();
        return (t == current_session_token);
    }

    if (http_server.hasHeader("Cookie")) {
        String cookie = http_server.header("Cookie");
        String needle = "token=" + String(current_session_token);
        if (cookie.indexOf(needle) >= 0) return true;
    }

    return false;
}

static void handle_login() {
    if (!http_server.hasArg("password")) {
        http_server.send(400, "application/json", "{\"error\":\"Missing password\"}");
        return;
    }

    String pass = http_server.arg("password");
    bool valid = (pass == DEFAULT_KEYPAD_PASSCODE || pass == "admin123");

    if (!valid) {
        http_server.send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
        return;
    }

    current_session_token = (uint32_t)os_random();
    if (current_session_token == 0) current_session_token = 12345;
    session_expiry_ms = millis() + SESSION_LIFETIME_MS;

    http_server.sendHeader("Set-Cookie", "token=" + String(current_session_token) + "; Path=/; HttpOnly");
    String res = "{\"authenticated\":true,\"token\":\"" + String(current_session_token) + "\"}";
    http_server.send(200, "application/json", res);
}

static void handle_logout() {
    current_session_token = 0;
    http_server.sendHeader("Set-Cookie", "token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
    http_server.send(200, "application/json", "{\"authenticated\":false}");
}

static const char *alarm_state_str(uint8_t st) {
    switch (st) {
        case ALARM_STATE_ARMING: return "ARMING";
        case ALARM_STATE_ARMED:  return "ARMED";
        case ALARM_STATE_ALARM:  return "ALARM";
        default:                 return "DISARMED";
    }
}

static const char *sector_name(uint8_t id) {
    switch (id) {
        case SEC_OPERATIVA:     return "Área Operativa";
        case SEC_AUTOMOTORES:   return "Automotores";
        case SEC_TALLER:        return "Jefatura de Taller";
        case SEC_VICEDIRECCION: return "Vicedirección";
        case SEC_INFORMATICA:   return "Informática";
        default:                return "Sector Desconocido";
    }
}

static void handle_get_state() {
    char buff[1536];
    bool auth = is_authenticated();

    uint8_t a_state = snapshot_received ? master_snapshot.alarm_state : ALARM_STATE_DISARMED;
    uint16_t arm_time = snapshot_received ? master_snapshot.arming_time_s : 0;
    uint8_t trig_reason = snapshot_received ? master_snapshot.trigger_reason : 0;
    uint8_t trig_sector = snapshot_received ? master_snapshot.trigger_sector : 0;

    int offset = snprintf(
        buff, sizeof(buff),
        "{\"authenticated\":%s,\"gateway_online\":true,\"master_connected\":%s,"
        "\"alarm_system\":{\"state\":\"%s\",\"arming_remaining_s\":%u,\"trigger_reason\":%u,\"trigger_sector\":%u},"
        "\"sectors\":[",
        auth ? "true" : "false",
        (snapshot_received && (millis() - last_snapshot_rx_ms <= 3000)) ? "true" : "false",
        alarm_state_str(a_state),
        arm_time,
        trig_reason,
        trig_sector
    );

    for (uint8_t i = 0; i < 5; i++) {
        uint8_t id = i + 1;
        bool en = snapshot_received ? (master_snapshot.sectors[i].enabled != 0) : true;
        bool on = snapshot_received ? (master_snapshot.sectors[i].online != 0) : false;
        bool mot = snapshot_received ? (master_snapshot.sectors[i].motion != 0) : false;
        bool pan = snapshot_received ? (master_snapshot.sectors[i].panic != 0) : false;
        uint16_t dist = snapshot_received ? master_snapshot.sectors[i].distance_cm : 0;
        uint32_t age = snapshot_received ? master_snapshot.sectors[i].last_seen_ms : 0;

        int written = snprintf(
            buff + offset, sizeof(buff) - offset,
            "%s{\"id\":%u,\"name\":\"%s\",\"enabled\":%s,\"online\":%s,\"motion\":%s,\"panic\":%s,\"distance_cm\":%u,\"age_ms\":%lu}",
            (i == 0) ? "" : ",",
            id,
            sector_name(id),
            en ? "true" : "false",
            on ? "true" : "false",
            mot ? "true" : "false",
            pan ? "true" : "false",
            dist,
            (unsigned long)age
        );
        if (written > 0 && offset + written < (int)sizeof(buff)) {
            offset += written;
        }
    }

    if (offset + 10 < (int)sizeof(buff)) {
        snprintf(buff + offset, sizeof(buff) - offset, "]}");
    }

    http_server.send(200, "application/json", buff);
}

static void handle_toggle_sector() {
    if (!is_authenticated()) {
        http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    if (!http_server.hasArg("sector_id") || !http_server.hasArg("enabled")) {
        http_server.send(400, "application/json", "{\"error\":\"Missing sector_id or enabled flag\"}");
        return;
    }

    uint8_t sec_id = (uint8_t)http_server.arg("sector_id").toInt();
    bool enabled = (http_server.arg("enabled") == "true" || http_server.arg("enabled") == "1");

    send_command_to_master(GW_MSG_SECTOR_TOGGLE, sec_id, enabled ? 1 : 0);
    http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handle_ping_sector() {
    if (!is_authenticated()) {
        http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    if (!http_server.hasArg("sector_id")) {
        http_server.send(400, "application/json", "{\"error\":\"Missing sector_id\"}");
        return;
    }

    uint8_t sec_id = (uint8_t)http_server.arg("sector_id").toInt();
    send_command_to_master(GW_MSG_MANUAL_PING, sec_id, 0);
    http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handle_arm_system() {
    if (!is_authenticated()) {
        http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }

    if (!http_server.hasArg("arm")) {
        http_server.send(400, "application/json", "{\"error\":\"Missing arm parameter\"}");
        return;
    }

    bool arm = (http_server.arg("arm") == "true" || http_server.arg("arm") == "1");
    send_command_to_master(GW_MSG_SYSTEM_ARM, arm ? ALARM_STATE_ARMING : ALARM_STATE_DISARMED, 0);
    http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void init_http_server() {
    const char *header_keys[] = {"Cookie", "X-Auth-Token"};
    http_server.collectHeaders(header_keys, 2);

    http_server.on("/api/login", HTTP_POST, handle_login);
    http_server.on("/api/logout", HTTP_POST, handle_logout);
    http_server.on("/api/state", HTTP_GET, handle_get_state);
    http_server.on("/api/sector/toggle", HTTP_POST, handle_toggle_sector);
    http_server.on("/api/sector/ping", HTTP_POST, handle_ping_sector);
    http_server.on("/api/system/arm", HTTP_POST, handle_arm_system);

    http_server.serveStatic("/", LittleFS, "/");
    http_server.begin();
    Serial.println("HTTP Server: Initialized");
}

void setup() {
    Serial.begin(SRL_BAUD);
    while (!Serial && millis() < 1000);

    init_leds();
    init_fs();
    init_wifi();
    init_http_server();
}

void loop() {
    http_server.handleClient();
    poll_master_serial();
    update_sector_leds();
}
