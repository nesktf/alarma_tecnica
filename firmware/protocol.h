#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/**
 * Protocolo de Comunicación Binaria: Nano <-> ESP8266
 * 
 * Diseño optimizado para bajo consumo de recursos y alta integridad.
 * Tamaño total de trama: 7 bytes.
 */

#define PROTOCOL_SYNC_BYTE 0xAA

typedef struct __attribute__((packed)) {
    uint8_t sync;      // Byte de sincronización (0xAA)
    uint8_t node_id;   // ID del nodo/área (1-255)
    uint16_t distance;  // Distancia en cm (Little Endian)
    uint8_t status;    // Bitmask: [0: Alarma, 1: Error, 2: Heartbeat, 3-7: Reservado]
    uint16_t crc;      // CRC-16-CCITT para verificación de integridad
} sensor_packet_t;

/**
 * Implementación de CRC-16-CCITT (XModem)
 * Polinomio: 0x1021
 */
static inline uint16_t calculate_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

#endif // PROTOCOL_H
