#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "network.h"
#include "sys_state.h"
#include "esp_webrtc.h"

/// @brief Initialize board
void board_init(void);

/// @brief Read the chip's internal temperature in degrees Celsius.
float board_get_temp(void);

#ifdef __cplusplus
}
#endif
