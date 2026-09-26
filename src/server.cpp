#include <stdint.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "./webpage.h"

// fill these with network data
static const char ssid[] = "Jef. de Taller EET3139";
static const char pswd[] = "777Gt3W!cuBP@";
static IPAddress local_ip{192, 168, 0, 108};
static IPAddress gateway {192, 168, 0, 1};
static IPAddress subnet {255, 255, 255, 0};

#define SRL_BAUD 9600
#define SERVER_PORT 80
#define TRIG_PIN 22
#define ECHO_PIN 21
#define WIFI_DEBUG

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
  server.begin();
  server.on(path, callback);
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

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);
#if 0
  init_wifi();
  init_server("/", +[]() {
    Serial.println("adasdasdasd");
    String response = WEB_SRC;
    server.send(200, "text/html", response);
    Serial.print("Server: GET response -> ");
    Serial.println(response);
  });
#endif
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  blink_led();
  Serial.println("init!");
}

void loop() {
  //server.handleClient();
  const float d = distance();
  Serial.print("Distance: ");
  Serial.println(d);
}
