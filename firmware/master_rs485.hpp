#ifndef MASTER_RS485_HPP
#define MASTER_RS485_HPP

#include "protocol.h"

void init_rs485_bus();
void poll_rs485_slaves();

#endif // MASTER_RS485_HPP