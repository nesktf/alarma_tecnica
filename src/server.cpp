#include <stdint.h>
#include <stdio.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "credentials.h"

static const char ssid[] = WIFI_SSID;
static const char pswd[] = WIFI_PASS;
static auto local_ip = SERVER_IP;
static auto gateway = SERVER_GATEWAY;
static auto subnet = SERVER_MASK;

#define SRL_BAUD 9600
#define SERVER_PORT 80
#define TRIG_PIN D6
#define ECHO_PIN D5
//#define WIFI_DEBUG

#define HALT() for(;;)

static ESP8266WebServer server{SERVER_PORT};
static bool filesystem_ready = false;
static bool measurement_valid = false;
static float last_distance_cm = 0;
static unsigned long last_measurement = 0;

static void init_fs() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS: Mount failed");
    HALT();
  }
  filesystem_ready = true;
  Serial.println("LittleFS: Initialized");
}

static void init_wifi() {
#ifdef WIFI_DEBUG
  WiFi.printDiag(Serial);
#endif
  if (!WiFi.config(local_ip, gateway, subnet)) {
    Serial.println("WiFi: STA configuration failed");
    HALT();
  }
  WiFi.begin(ssid, pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi: Connected -> ");
  Serial.println(WiFi.localIP());
}

static void send_state() {
  char distance[16];
  if (measurement_valid) {
    snprintf(distance, sizeof(distance), "%.1f", last_distance_cm);
  } else {
    snprintf(distance, sizeof(distance), "null");
  }

  char response[384];
  const int response_length = snprintf(
      response,
      sizeof(response),
      "{\"simulator\":false,\"board\":\"NodeMCU ESP8266\",\"state\":\"running\","
      "\"wifi\":true,\"filesystem\":%s,\"uptime_ms\":%lu,"
      "\"sensor\":{\"valid\":%s,\"distance_cm\":%s,"
      "\"age_ms\":%ld,\"echo_timeout_us\":30000},"
      "\"pins\":{\"trigger_gpio\":%u,\"echo_gpio\":%u,\"trigger_label\":\"D6\",\"echo_label\":\"D5\"},"
      "\"limits\":{\"cpu_mhz\":80,\"flash_bytes\":4194304,\"dram_bytes\":80192,\"iram_bytes\":65536}}",
      filesystem_ready ? "true" : "false",
      (unsigned long)millis(),
      measurement_valid ? "true" : "false",
      distance,
      measurement_valid ? (long)(millis() - last_measurement) : -1L,
      (unsigned)TRIG_PIN,
      (unsigned)ECHO_PIN);
  if (response_length < 0 || (size_t)response_length >= sizeof(response)) {
    server.send(500, "application/json", "{\"error\":\"State response overflow\"}");
    return;
  }
  server.send(200, "application/json", response);
}

static void init_server() {
  server.on("/api/state", HTTP_GET, send_state);
  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println("Server: Initialized static file serving from LittleFS");
}

static void blink_led() {
  pinMode(LED_BUILTIN, OUTPUT);

  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
}

#define SOUND_VEL 0.034

static bool read_distance(float& distance_cm) {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);

  const auto duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) {
    return false;
  }
  distance_cm = static_cast<float>(duration) * SOUND_VEL / 2.0f;
  return true;
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  init_fs();
  init_wifi();
  init_server();
  blink_led();
}

void loop() {
  server.handleClient();

  if (millis() - last_measurement >= 1000) {
    last_measurement = millis();
    measurement_valid = read_distance(last_distance_cm);
    if (measurement_valid) {
      Serial.print("Distance: ");
      Serial.println(last_distance_cm);
    } else {
      Serial.println("Distance: no echo");
    }
  }
}
