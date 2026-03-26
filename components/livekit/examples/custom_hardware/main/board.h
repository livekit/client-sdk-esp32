#pragma once

#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the Waveshare ESP32-S3-Touch-LCD-1.83 board for audio.
/// Sets up I2C, I2S, ES8311 (DAC) and ES7210 (ADC).
void board_init(void);

/// Get the playback codec device handle (ES8311).
esp_codec_dev_handle_t get_playback_handle(void);

/// Get the record codec device handle (ES7210).
esp_codec_dev_handle_t get_record_handle(void);

#ifdef __cplusplus
}
#endif
