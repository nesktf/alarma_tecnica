#ifndef SERVER_HPP
#define SERVER_HPP

#include "config.h"
#include "protocol.h"

#include <pins_arduino.h>

extern gw_state_packet_t master_snapshot;
extern bool snapshot_received;
extern uint32_t last_snapshot_rx_ms;

static constexpr uint8_t SECTOR_LEDS[5] = {NODEMCU_LED_SEC_OPERATIVA, NODEMCU_LED_SEC_AUTOMOTORES,
                                           NODEMCU_LED_SEC_TALLER, NODEMCU_LED_SEC_VICEDIRECCION,
                                           NODEMCU_LED_SEC_INFORMATICA};

void send_command_to_master(uint8_t msg_type, uint8_t param1, uint8_t param2);

#endif // SERVER_HPP
