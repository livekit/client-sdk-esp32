#pragma once

#include "esp_codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize board.
void board_init(void);

/// Return the codec device used for speaker rendering.
esp_codec_dev_handle_t board_get_playback_handle(void);

/// Return the codec device used for microphone capture.
esp_codec_dev_handle_t board_get_record_handle(void);

/// Return the playback channel count used by the selected board profile.
unsigned board_get_playback_channels(void);

#ifdef __cplusplus
}
#endif
