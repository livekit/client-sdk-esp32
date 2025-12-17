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

#ifdef __cplusplus
}
#endif
