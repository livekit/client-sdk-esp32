#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "settings.h"
#include "media_sys.h"
#include "network.h"
#include "sys_state.h"
#include "esp_webrtc.h"

typedef enum {
    BOARD_LED_RED = 0,
    BOARD_LED_GREEN = 1
} board_led_t;

/// @brief Initialize board
void board_init(void);

/// @brief Read the chip's internal temperature in degrees Celsius.
float board_get_temp(void);

/// @brief Set the state of an on-board LED.
void board_set_led_state(board_led_t led, bool state);

#ifdef __cplusplus
}
#endif
