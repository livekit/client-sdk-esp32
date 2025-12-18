#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize board.
void board_init(void);

/// Get initialized microphone codec device handle (input).
///
/// Returned as an opaque pointer to avoid forcing codec headers into all translation units.
void *board_get_mic_handle(void);

/// Get initialized speaker codec device handle (output).
///
/// Returned as an opaque pointer to avoid forcing codec headers into all translation units.
void *board_get_speaker_handle(void);

/// Update the UI audio visualizer level from the audio render path.
///
/// @param level Normalized amplitude in [0.0, 1.0]. Values outside are clamped.
///
/// @note This is safe to call from non-LVGL threads/contexts; it does not call LVGL APIs.
void board_visualizer_set_level(float level);

/// Update the UI mic input visualizer level (top indicator).
///
/// When unmuted:
/// - silent/low: show a blue dot
/// - speaking: show center-out blue gradient bars (only once wider than the dot)
///
/// When muted:
/// - show a red dot
///
/// @param level Normalized amplitude in [0.0, 1.0]. Values outside are clamped.
///
/// @note Safe to call from non-LVGL threads/contexts; it does not call LVGL APIs.
void board_mic_visualizer_set_level(float level);

/// Update the mic mute indicator UI (top indicator styling).
///
/// @note Safe to call from non-LVGL threads; this function takes the LVGL lock.
void board_set_mic_muted(bool muted);

#ifdef __cplusplus
}
#endif
