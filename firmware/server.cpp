#include <stdint.h>
#include <stdio.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "sensor.hpp"
#include "credentials.h"

#define SRL_BAUD 9600
#define SERVER_PORT 80
//#define WIFI_DEBUG

#define HALT() for(;;)

static ESP8266WebServer server{SERVER_PORT};

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
  const auto local_ip = SERVER_IP;
  const auto gateway = SERVER_GATEWAY;
  const auto subnet = SERVER_MASK;
  if (!WiFi.config(local_ip, gateway, subnet)) {
    Serial.println("WiFi: STA configuration failed");
    HALT();
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi: Connected -> ");
  Serial.println(WiFi.localIP());
}

static void send_state() {
  char response[1024];
  const auto len = format_sensor_state(response, sizeof(response));
  if (len < 0 || (size_t)len >= sizeof(response)) {
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

static void init_led() {
  pinMode(LED_BUILTIN, OUTPUT);
}

static void blink_led() {
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  init_fs();
  init_wifi();
  init_server();
  init_led();
  init_sensor();
  blink_led();
}

void loop() {
  server.handleClient();
  poll_sensor();
}
