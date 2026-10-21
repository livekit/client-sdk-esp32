#pragma once

#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the GMIC HA-TOYMD module for audio.
/// Sets up I2C, I2S and the ES8311 codec (playback + capture).
void board_init(void);

/// Get the playback codec device handle (ES8311).
esp_codec_dev_handle_t get_playback_handle(void);

/// Get the record codec device handle (ES8311).
esp_codec_dev_handle_t get_record_handle(void);

#ifdef __cplusplus
}
#endif
