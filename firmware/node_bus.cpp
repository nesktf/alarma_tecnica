#include "node_bus.hpp"
#include <Arduino.h>
#include "protocol.h"

// ---------------------------------------------------------------------------
// Estado interno del bus
// ---------------------------------------------------------------------------

static node_state_t node_states[NODE_BUS_MAX_NODES + 1]; // índice 1..MAX
static uint32_t last_poll_ms = 0;
static uint8_t  current_node = 0;
static uint32_t poll_start_ms = 0;
static bool     de_state = false; // false = recepción (LOW), true = transmisión (HIGH)

// ---------------------------------------------------------------------------
// Utilidades auxiliares
// ---------------------------------------------------------------------------

static void set_de(bool high) {
    if (high != de_state) {
        de_state = high;
        digitalWrite(NODE_BUS_DE_PIN, high ? HIGH : LOW);
    }
}

static bool read_serial_packet(sensor_packet_t *out) {
    if (Serial.available() < sizeof(sensor_packet_t)) {
        return false;
    }

    // Leer los 7 bytes exactos
    uint8_t raw[sizeof(sensor_packet_t)];
    Serial.readBytes(raw, sizeof(raw));

    // Verificar byte de sincronización
    if (raw[0] != PROTOCOL_SYNC_BYTE) return false;

    // Calcular y verificar CRC en formato little-endian explícito
    uint16_t calc_crc = calculate_crc16(raw, sizeof(sensor_packet_t) - sizeof(uint16_t));
    uint16_t packet_crc = (uint16_t)raw[5] | ((uint16_t)raw[6] << 8);
    if (calc_crc != packet_crc) {
        return false; // CRC inválido
    }

    // Desempaquetar explícitamente evitando accesos no alineados
    if (out != NULL) {
        out->sync = raw[0];
        out->node_id = raw[1];
        out->distance = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
        out->status = raw[4];
        out->crc = packet_crc;
    }
    return true;
}

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

void node_bus_init() {
    pinMode(NODE_BUS_DE_PIN, OUTPUT);
    set_de(false); // recepción por defecto
    Serial.begin(9600);

    // Inicializar estados
    for (uint8_t i = 1; i <= NODE_BUS_MAX_NODES; ++i) {
        node_states[i].node_id = i;
        node_states[i].online = false;
        node_states[i].rx_ok = 0;
        node_states[i].rx_err = 0;
    }
}

static uint8_t last_polled_node = 0;

void node_bus_poll() {
    const uint32_t now = millis();
    if (now - last_poll_ms < NODE_BUS_POLL_MS) return;
    last_poll_ms = now;

    // Si ya estamos esperando respuesta, solo verificar timeout
    if (current_node > 0) {
        if (now - poll_start_ms >= NODE_BUS_TIMEOUT_MS) {
            // Timeout: marcar nodo como offline
            node_states[current_node].online = false;
            node_states[current_node].rx_err++;
            current_node = 0;
            set_de(false);
            return;
        }
        // Continuar esperando
        return;
    }

    // Selección circular (round-robin) de nodos 1..NODE_BUS_MAX_NODES
    last_polled_node = (last_polled_node % NODE_BUS_MAX_NODES) + 1;
    current_node = last_polled_node;

    // Enviar comando de polling: POLL_CMD | node_id
    set_de(true);
    Serial.write(NODE_BUS_POLL_CMD | current_node);
    Serial.flush();
    set_de(false);
    poll_start_ms = now;
}

const node_state_t* node_bus_get_state(uint8_t node_id) {
    if (node_id == 0 || node_id > NODE_BUS_MAX_NODES) return nullptr;
    return &node_states[node_id];
}

uint8_t node_bus_online_count() {
    uint8_t cnt = 0;
    for (uint8_t i = 1; i <= NODE_BUS_MAX_NODES; ++i) {
        if (node_states[i].online) cnt++;
    }
    return cnt;
}

// ---------------------------------------------------------------------------
// Hook: Llamar desde loop() para procesar respuestas recibidas
// ---------------------------------------------------------------------------
void node_bus_check() {
    if (current_node == 0) return;

    if (Serial.available() < sizeof(sensor_packet_t)) {
        return;
    }

    sensor_packet_t pkt;
    if (read_serial_packet(&pkt)) {
        if (pkt.node_id == current_node) {
            node_states[current_node].distance_cm = pkt.distance;
            node_states[current_node].status = pkt.status;
            node_states[current_node].last_seen_ms = millis();
            node_states[current_node].online = true;
            node_states[current_node].rx_ok++;
        } else {
            node_states[current_node].rx_err++;
        }

        while (Serial.available()) Serial.read();
    } else {
        node_states[current_node].rx_err++;
    }

    current_node = 0;
}
