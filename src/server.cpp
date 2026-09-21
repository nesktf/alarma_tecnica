#include <stdint.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "./webpage.h"

// fill these with network data
static const char ssid[] = "";
static const char pswd[] = "";
static IPAddress local_ip{192, 168, 0, 53};
static IPAddress gateway {192, 168, 0, 1};
static IPAddress subnet {255, 255, 255, 0};

#define SRL_BAUD 9600
#define SERVER_PORT 80
//#define WIFI_DEBUG

#define HALT() for(;;)

static ESP8266WebServer server{SERVER_PORT};

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

static void init_server(const char* path, void(*callback)()) {
  server.on(path, callback);
  server.begin();
  Serial.print("Server: Initialized -> ");
  Serial.println(path);
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


void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);
  init_wifi();
  init_server("/", +[]() {
    String response = WEB_SRC;
    server.send(200, "text/html", response);
    Serial.print("Server: GET response -> ");
    Serial.println(response);
  });
  blink_led();
}

void loop() {
  server.handleClient();
}
