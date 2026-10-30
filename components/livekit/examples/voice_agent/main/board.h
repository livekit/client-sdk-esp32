#pragma once

#include <stdbool.h>
#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize board.
void board_init(void);

/// Return the board's playback codec device.
esp_codec_dev_handle_t board_get_playback_handle(void);

/// Return the board's capture codec device.
esp_codec_dev_handle_t board_get_record_handle(void);

/// Return the playback channel count used by the selected board profile.
unsigned board_get_playback_channels(void);

/// Read the chip's internal temperature in degrees Celsius.
float board_get_temp(void);

/// Set one of the board's indicator LEDs.
///
/// Returns false when the requested LED is unavailable on the selected board.
bool board_set_led_state(const char *color, bool state);

/// Return a JSON description of the selected board and capabilities exposed by
/// this example.
const char *board_get_info_json(void);

#ifdef __cplusplus
}
#endif
