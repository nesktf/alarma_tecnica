#include <stdint.h>
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
#define TRIG_PIN 22
#define ECHO_PIN 21
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

static void init_server() {
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

#if 0
#define SOUND_VEL 0.034

static float distance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);

  const auto duration = pulseIn(ECHO_PIN, HIGH);
  const float distance = (float)duration * SOUND_VEL/2.f;
  
  delay(1000);
  return distance;
}
#endif

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);
  init_fs();
  init_wifi();
  init_server();
  blink_led();
}

void loop() {
  server.handleClient();
}
