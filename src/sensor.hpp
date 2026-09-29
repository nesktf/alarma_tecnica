#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <stdlib.h>

extern bool filesystem_ready;

void init_sensor();
int format_sensor_state(char* buff, size_t sz);
void poll_sensor();

#endif // SENSOR_HPP
