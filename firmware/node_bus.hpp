#ifndef NODE_BUS_HPP
#define NODE_BUS_HPP

#include <stdint.h>
#include "protocol.h"

// ---------------------------------------------------------------------------
// Bus RS485 de nodos (Nano → ESP8266)
//
// El ESP8266 actúa como Maestro Half-Duplex. Para cada ciclo de polling:
//   1. Envía 1 byte de solicitud: POLL_CMD | node_id  (ej. 0xB0 | 0x01 = 0xB1)
//   2. Espera hasta NODE_BUS_TIMEOUT_MS la respuesta de 7 bytes (sensor_packet_t)
//   3. Verifica SYNC + CRC16; descarta si falla.
//
// Constantes:
//   NODE_BUS_MAX_NODES  : cantidad máxima de nodos soportados
//   NODE_BUS_POLL_MS    : intervalo entre consultas completas al bus (ms)
//   NODE_BUS_TIMEOUT_MS : timeout por respuesta de un nodo (ms)
//   NODE_BUS_DE_PIN     : GPIO del pin DE/!RE del transceptor RS485 MAX485
// ---------------------------------------------------------------------------

#define NODE_BUS_MAX_NODES    9
#define NODE_BUS_POLL_MS      100
#define NODE_BUS_TIMEOUT_MS   50
#define NODE_BUS_DE_PIN       D7    // GPIO13 — DE/!RE del MAX485

// Comando de polling maestro → esclavo
#define NODE_BUS_POLL_CMD     0xB0

// Estado por nodo actualizado en cada ciclo de bus
typedef struct {
    uint8_t  node_id;
    uint16_t distance_cm;
    uint8_t  status;          // mismo bitmask que sensor_packet_t
    uint32_t last_seen_ms;    // millis() del último paquete válido recibido
    bool     online;
    uint32_t rx_ok;           // paquetes válidos acumulados
    uint32_t rx_err;          // paquetes con error CRC o SYNC
} node_state_t;

// ---------------------------------------------------------------------------
// API pública
// ---------------------------------------------------------------------------

// Inicializa el bus (Serial1 a 9600 baud, pin DE en OUTPUT LOW).
void node_bus_init();

// Ejecuta un ciclo completo de polling (llamar desde loop()).
// Sólo actúa cuando NODE_BUS_POLL_MS ha transcurrido.
void node_bus_poll();

// Procesa respuestas recibidas en el bus RS485 (llamar desde loop()).
void node_bus_check();

// Retorna un puntero de sólo lectura al estado del nodo (1-indexed).
// Devuelve nullptr si node_id está fuera de rango.
const node_state_t* node_bus_get_state(uint8_t node_id);

// Número de nodos que respondieron en el último ciclo completo.
uint8_t node_bus_online_count();

#endif // NODE_BUS_HPP
