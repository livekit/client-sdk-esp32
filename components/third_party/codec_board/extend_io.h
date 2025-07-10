#include <stdint.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int extend_io_init(uint8_t io_i2c_port);
int extend_io_set_pin_dir(int16_t pin, bool output);
int extend_io_set_pin_state(int16_t pin, bool high);
int16_t extend_io_get_hw_gpio(int16_t pin);

#ifdef __cplusplus
}
#endif