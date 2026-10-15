#include "server_http.hpp"

#include "server.hpp"

#include "config.h"
#include "credentials.h"
#include "protocol.h"

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

#if !defined(SERVER_IP) || !defined(SERVER_GATEWAY) || !defined(SERVER_MASK)
#error "Server config not defined"
#endif

#if !defined(SERVER_LOGIN_PASS_HASH)
#error "Server login hash not defined"
#endif

namespace {

#define SERVER_PORT 80
ESP8266WebServer http_server{SERVER_PORT};

uint32_t current_session_token = 0;
uint32_t session_expiry_ms = 0;
#define SESSION_LIFETIME_MS 3600000UL

bool is_authenticated() {
  if (current_session_token == 0) {
    return false;
  }
  if (millis() > session_expiry_ms) {
    current_session_token = 0;
    return false;
  }

  if (http_server.hasHeader("X-Auth-Token")) {
    const uint32_t t = (uint32_t)http_server.header("X-Auth-Token").toInt();
    return (t == current_session_token);
  }

  if (http_server.hasHeader("Cookie")) {
    const String cookie = http_server.header("Cookie");
    const String needle = "token=" + String(current_session_token);
    if (cookie.indexOf(needle) >= 0) {
      return true;
    }
  }

  return false;
}

void handle_get_state() {
  const auto alarm_state_str = [](uint8_t st) -> const char* {
    switch (st) {
      case ALARM_STATE_ARMING:
        return "ARMING";
      case ALARM_STATE_ARMED:
        return "ARMED";
      case ALARM_STATE_ALARM:
        return "ALARM";
      default:
        return "DISARMED";
    }
  };

  const auto sector_name = [](uint8_t id) -> const char* {
    switch (id) {
      case SEC_OPERATIVA:
        return "Área Operativa";
      case SEC_AUTOMOTORES:
        return "Automotores";
      case SEC_TALLER:
        return "Jefatura de Taller";
      case SEC_VICEDIRECCION:
        return "Vicedirección";
      case SEC_INFORMATICA:
        return "Informática";
      default:
        return "Sector Desconocido";
    }
  };

  const bool auth = is_authenticated();
  const uint8_t a_state = snapshot_received ? master_snapshot.alarm_state : ALARM_STATE_DISARMED;
  const uint16_t arm_time = snapshot_received ? master_snapshot.arming_time_s : 0;
  const uint8_t trig_reason = snapshot_received ? master_snapshot.trigger_reason : 0;
  const uint8_t trig_sector = snapshot_received ? master_snapshot.trigger_sector : 0;

  char buff[1536];
  int offset =
    snprintf(buff, sizeof(buff),
             "{\"authenticated\":%s,\"gateway_online\":true,\"master_connected\":%s,"
             "\"alarm_system\":{\"state\":\"%s\",\"arming_remaining_s\":%u,\"trigger_reason\":%u,"
             "\"trigger_sector\":%u},"
             "\"sectors\":[",
             auth ? "true" : "false",
             (snapshot_received && (millis() - last_snapshot_rx_ms <= 3000)) ? "true" : "false",
             alarm_state_str(a_state), arm_time, trig_reason, trig_sector);

  for (uint8_t i = 0; i < 5; i++) {
    const uint8_t id = i + 1;
    const bool en = snapshot_received ? (master_snapshot.sectors[i].enabled != 0) : true;
    const bool on = snapshot_received ? (master_snapshot.sectors[i].online != 0) : false;
    const bool mot = snapshot_received ? (master_snapshot.sectors[i].motion != 0) : false;
    const bool pan = snapshot_received ? (master_snapshot.sectors[i].panic != 0) : false;
    const uint16_t dist = snapshot_received ? master_snapshot.sectors[i].distance_cm : 0;
    const uint32_t age = snapshot_received ? master_snapshot.sectors[i].last_seen_ms : 0;

    const int written = snprintf(
      buff + offset, sizeof(buff) - offset,
      "%s{\"id\":%u,\"name\":\"%s\",\"enabled\":%s,\"online\":%s,\"motion\":%"
      "s,\"panic\":%s,\"distance_cm\":%u,\"age_ms\":%lu}",
      (i == 0) ? "" : ",", id, sector_name(id), en ? "true" : "false", on ? "true" : "false",
      mot ? "true" : "false", pan ? "true" : "false", dist, (unsigned long)age);
    if (written > 0 && offset + written < (int)sizeof(buff)) {
      offset += written;
    }
  }

  if (offset + 10 < (int)sizeof(buff)) {
    snprintf(buff + offset, sizeof(buff) - offset, "]}");
  }

  http_server.send(200, "application/json", buff);
}

void handle_login() {
  if (!http_server.hasArg("password")) {
    http_server.send(400, "application/json", "{\"error\":\"Missing password\"}");
    return;
  }

  const String pass_hash = http_server.arg("password");
  const bool valid = (pass_hash == SERVER_LOGIN_PASS_HASH
                      || pass_hash == MASTER_KEYPASS_PASSCODE_HASH
  );
  if (!valid) {
    http_server.send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
    return;
  }

  current_session_token = (uint32_t)os_random();
  if (current_session_token == 0) {
    current_session_token = 12345;
  }
  session_expiry_ms = millis() + SESSION_LIFETIME_MS;

  http_server.sendHeader("Set-Cookie",
                         "token=" + String(current_session_token) + "; Path=/; HttpOnly");
  const String res =
    "{\"authenticated\":true,\"token\":\"" + String(current_session_token) + "\"}";
  http_server.send(200, "application/json", res);
}

void handle_logout() {
  current_session_token = 0;
  http_server.sendHeader("Set-Cookie", "token=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  http_server.send(200, "application/json", "{\"authenticated\":false}");
}

void handle_sector_toggle() {
  if (!is_authenticated()) {
    http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }

  if (!http_server.hasArg("sector_id") || !http_server.hasArg("enabled")) {
    http_server.send(400, "application/json", "{\"error\":\"Missing sector_id or enabled flag\"}");
    return;
  }

  const uint8_t sec_id = (uint8_t)http_server.arg("sector_id").toInt();
  const bool enabled = (http_server.arg("enabled") == "true" || http_server.arg("enabled") == "1");
  send_command_to_master(GW_MSG_SECTOR_TOGGLE, sec_id, enabled ? 1 : 0);
  http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handle_ping() {
  if (!is_authenticated()) {
    http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }

  if (!http_server.hasArg("sector_id")) {
    http_server.send(400, "application/json", "{\"error\":\"Missing sector_id\"}");
    return;
  }

  const uint8_t sec_id = (uint8_t)http_server.arg("sector_id").toInt();
  send_command_to_master(GW_MSG_MANUAL_PING, sec_id, 0);
  http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handle_arm() {
  if (!is_authenticated()) {
    http_server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }

  if (!http_server.hasArg("arm")) {
    http_server.send(400, "application/json", "{\"error\":\"Missing arm parameter\"}");
    return;
  }

  const bool armed = (http_server.arg("arm") == "true" || http_server.arg("arm") == "1");
  send_command_to_master(GW_MSG_SYSTEM_ARM, armed ? ALARM_STATE_ARMING : ALARM_STATE_DISARMED, 0);
  http_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

} // namespace

void init_http_server() {
  // init wifi
#ifdef WIFI_DEBUG
  WiFi.printDiag(Serial);
#endif

  const auto local_ip = SERVER_IP;
  const auto gateway = SERVER_GATEWAY;
  const auto subnet = SERVER_MASK;
  WiFi.config(local_ip, gateway, subnet);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);

  const uint32_t start_wifi = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start_wifi < 15000) {
    delay(300);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi: Connection timeout, continuing...");
  }

  // init the actual http server
  http_server.collectHeaders("Cookie", "X-Auth-Token");

  http_server.on("/api/login", HTTP_POST, handle_login);
  http_server.on("/api/logout", HTTP_POST, handle_logout);
  http_server.on("/api/state", HTTP_GET, handle_get_state);
  http_server.on("/api/sector/toggle", HTTP_POST, handle_sector_toggle);
  http_server.on("/api/sector/ping", HTTP_POST, handle_ping);
  http_server.on("/api/system/arm", HTTP_POST, handle_arm);

  http_server.serveStatic("/", LittleFS, "/");
  http_server.begin();
  Serial.println("HTTP Server: Initialized");
}

void handle_server_client() {
  http_server.handleClient();
}
