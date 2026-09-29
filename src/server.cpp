#include <stdint.h>
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

static void init_server() {
  server.serveStatic("/", LittleFS, "/");
  server.begin();
  Serial.println("Server: Initialized static file serving from LittleFS");
}

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  init_fs();
  init_wifi();
  init_server();
  init_led();
  //init_sensor();
  blink_led();
}

void loop() {
  server.handleClient();
  //print_sensor();
}
