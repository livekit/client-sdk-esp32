#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Layout of the audio channels read from the board's record device, as
/// required by the AEC capture source (see `esp_capture_audio_aec_src_cfg_t`).
typedef struct {
    uint8_t channel;      /// Number of channels to read
    uint8_t channel_mask; /// Mask of channels to use (0 for all)
} board_capture_layout_t;

/// Initialize board.
void board_init(void);

/// Get the audio capture channel layout for the board.
board_capture_layout_t board_get_capture_layout(void);

#ifdef __cplusplus
}
#endif
