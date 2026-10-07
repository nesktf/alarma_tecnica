#include "sensor.hpp"
#include "node_bus.hpp"
#include <ESP8266WiFi.h>
#include "credentials.h"

#define TRIG_PIN D6
#define ECHO_PIN D5

#define SOUND_VEL 0.034

void init_sensor() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

bool filesystem_ready = false;
static bool measurement_valid = false;
static float last_distance_cm = 0;
static unsigned long last_measurement = 0;

static bool read_sensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);

  const auto duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (!duration) {
    return false;
  }
  last_distance_cm = static_cast<float>(duration) * SOUND_VEL / 2.0f;
  return true;
}

void poll_sensor() {
  if (millis() - last_measurement >= 1000) {
    last_measurement = millis();
    measurement_valid = read_sensor();
    if (measurement_valid) {
      Serial.print("Distance: ");
      Serial.println(last_distance_cm);
    } else {
      Serial.println("Distance: no echo");
    }
  }
}

int format_sensor_state(char* buff, size_t sz) {
  char distance[16];
  if (measurement_valid) {
    snprintf(distance, sizeof(distance), "%.1f", last_distance_cm);
  } else {
    snprintf(distance, sizeof(distance), "null");
  }

  char age_str[16];
  if (measurement_valid) {
    snprintf(age_str, sizeof(age_str), "%ld", (long)(millis() - last_measurement));
  } else {
    snprintf(age_str, sizeof(age_str), "null");
  }

  bool is_wifi_connected = false;
#ifdef ESP8266
  is_wifi_connected = (WiFi.status() == WL_CONNECTED);
#else
  is_wifi_connected = true;
#endif

  const char* sys_state = "running";
  if (!filesystem_ready) {
    sys_state = "halted_filesystem";
  } else if (!is_wifi_connected) {
    sys_state = "waiting_wifi";
  }

  // Construir la cadena de nodos del bus RS485 (1024 bytes garantizan 9 nodos completos sin truncar)
  char nodes_str[1024] = "";
  size_t nodes_len = 0;
  bool first = true;
  for (uint8_t i = 1; i <= 9; ++i) {
    const node_state_t *ns = node_bus_get_state(i);
    if (!ns) continue;
    
    char part[128];
    int n = snprintf(
      part, sizeof(part),
      "%s{\"node_id\":%u,\"distance_cm\":%u,\"status\":%u,\"online\":%s,\"last_seen_ms\":%lu}",
      first ? "" : ",",
      ns->node_id,
      ns->distance_cm,
      ns->status,
      ns->online ? "true" : "false",
      (unsigned long)ns->last_seen_ms
    );
    
    if (n <= 0 || nodes_len + (size_t)n >= sizeof(nodes_str)) {
      // Truncamiento detectado en el buffer de nodos: retornar error explícito
      return -1;
    }
    memcpy(nodes_str + nodes_len, part, n);
    nodes_len += n;
    first = false;
  }

  const char* ip_str = "127.0.0.1";
#ifdef SERVER_IP_STR
  ip_str = SERVER_IP_STR;
#endif

  int written = snprintf(
    buff,
    sz,
    "{\"simulator\":false,\"board\":\"NodeMCU ESP8266\",\"state\":\"%s\","
    "\"uptime_ms\":%lu,\"wifi\":%s,\"filesystem\":%s,"
    "\"sensor\":{\"valid\":%s,\"distance_cm\":%s,\"age_ms\":%s,\"echo_timeout_us\":30000},"
    "\"pins\":{\"trigger_gpio\":12,\"echo_gpio\":14,\"trigger_label\":\"D6\",\"echo_label\":\"D5\"},"
    "\"limits\":{\"cpu_mhz\":80,\"flash_bytes\":4194304,\"dram_bytes\":80192,\"iram_bytes\":65536},"
    "\"server\":{\"status\":\"%s\",\"host\":\"%s\",\"port\":80,\"base_url\":\"http://%s\",\"last_request\":\"/api/state\",\"routes\":[\"/\",\"/api/state\"]},"
    "\"alarm_system\":{\"state\":\"DISARMED\",\"arming_remaining_s\":0.0,\"v_bus\":12.4,\"v_bus_status\":\"normal\","
    "\"active_areas\":[],\"no_response_areas\":[],"
    "\"sectors\":["
    "{\"id\":1,\"name\":\"Área Operativa\",\"node\":\"central\",\"has_panic\":true,\"motion\":false,\"active\":false,\"no_response\":false,\"status\":\"normal\"},"
    "{\"id\":2,\"name\":\"Automotores\",\"node\":\"remote\",\"has_panic\":false,\"motion\":false,\"active\":false,\"no_response\":false,\"status\":\"normal\"},"
    "{\"id\":3,\"name\":\"Jefatura de Taller\",\"node\":\"remote\",\"has_panic\":true,\"motion\":false,\"active\":false,\"no_response\":false,\"status\":\"normal\"},"
    "{\"id\":4,\"name\":\"Vicedirección\",\"node\":\"remote\",\"has_panic\":true,\"motion\":false,\"active\":false,\"no_response\":false,\"status\":\"normal\"},"
    "{\"id\":5,\"name\":\"Informática\",\"node\":\"remote\",\"has_panic\":false,\"motion\":false,\"active\":false,\"no_response\":false,\"status\":\"normal\"}"
    "],"
    "\"recent_events\":[]},"
    "\"ws\":{\"connected\":false,\"port\":0},"
    "\"auth\":{\"authenticated\":false,\"method\":\"none\"},"
    "\"nodes\":[%s]}",
    sys_state,
    (unsigned long)millis(),
    is_wifi_connected ? "true" : "false",
    filesystem_ready ? "true" : "false",
    measurement_valid ? "true" : "false",
    distance,
    age_str,
    is_wifi_connected ? "running" : "degraded",
    ip_str,
    ip_str,
    nodes_str
  );

  if (written < 0 || (size_t)written >= sz) {
    return -1; // Buffer de salida truncado
  }
  return written;
}

