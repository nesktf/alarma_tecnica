#ifndef NODE_SENSOR_HPP
#define NODE_SENSOR_HPP

#include <stdint.h>

void init_ultrasonic(uint8_t trig_pin, uint8_t echo_pin);
float read_ultrasonic(uint8_t trig_pin, uint8_t echo_pin);

#endif // NODE_SENSOR_HPP
