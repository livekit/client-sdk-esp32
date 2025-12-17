#pragma once

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

#ifdef __cplusplus
}
#endif
