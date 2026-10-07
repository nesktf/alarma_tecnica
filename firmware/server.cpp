#include <stdint.h>
#include <stdio.h>
#include <pins_arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>

#include "sensor.hpp"
#include "node_bus.hpp"
#include "credentials.h"

#define SRL_BAUD 9600
#define SERVER_PORT 80
#define WS_PORT 81

// ---------------------------------------------------------------------------
// HTTP Servidor Web
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Servidor HTTP (archivos estáticos + /api/state para compatibilidad)
// ---------------------------------------------------------------------------

static ESP8266WebServer http_server{SERVER_PORT};
// WebSocket support disabled to resolve build dependency issues.
// static ESP8266WebSocket web_socket{WS_PORT};

static bool ws_connected = false;

// static void on_ws_data(ESP8266WebSocket client, uint8_t *data, size_t len) {
//   // El cliente puede enviar un comando simple por WS.
//   // Comandos soportados: "ping" -> respuesta "pong"
//   if (len >= 4 && strncmp((char *)data, "ping", 4) == 0) {
//     client.sendString("pong");
//   }
// }
//
// static void on_ws_connect(ESP8266WebSocket client, uint8_t *data, size_t len) {
//   ws_connected = true;
//   Serial.println("WebSocket: Cliente conectado");
// }
//
// static void on_ws_disconnect(ESP8266WebSocket client) {
//   ws_connected = false;
//   Serial.println("WebSocket: Cliente desconectado");
// }

// ---------------------------------------------------------------------------
// Inicialización
// ---------------------------------------------------------------------------

static void init_fs() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS: Mount failed");
    while(1);
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
    while(1);
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
  char response[1536];
  const auto len = format_sensor_state(response, sizeof(response));
  if (len < 0 || (size_t)len >= sizeof(response)) {
    http_server.send(500, "application/json", "{\"error\":\"State response overflow\"}");
    return;
  }
  http_server.send(200, "application/json", response);
}

static void init_http_server() {
  http_server.on("/api/state", HTTP_GET, send_state);
  http_server.serveStatic("/", LittleFS, "/");
  http_server.begin();
  Serial.println("HTTP Server: Initialized static file serving from LittleFS");
}

static void init_websocket_server() {
  // WebSocket support disabled to resolve build dependency issues.
  // web_socket.onData(on_ws_data);
  // web_socket.onConnect(on_ws_connect);
  // web_socket.onDisconnect(on_ws_disconnect);
  // web_socket.begin();
  // Serial.println("WebSocket Server: Initialized on port " + String(WS_PORT));
}

static void init_node_bus() {
  node_bus_init();
  Serial.println("Node Bus: Initialized (MAX485 DE on D7, 9600 baud)");
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

// ---------------------------------------------------------------------------
// Loop principal
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(SRL_BAUD);
  while (!Serial);

  init_fs();
  init_wifi();
  init_http_server();
  init_websocket_server();
  init_node_bus();
  init_led();
  init_sensor();
  blink_led();
}

void loop() {
  http_server.handleClient();
  // WebSocket support disabled to resolve build dependency issues.
  // web_socket.accept();
  // web_socket.broadcastData((uint8_t *)"", 0); // mantener conexión viva

  node_bus_poll();
  node_bus_check();
  poll_sensor();
}
