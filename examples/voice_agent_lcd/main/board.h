#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize board.
void board_init(void);

/// Read the chip's internal temperature in degrees Celsius.
float board_get_temp(void);

#ifdef __cplusplus
}
#endif
