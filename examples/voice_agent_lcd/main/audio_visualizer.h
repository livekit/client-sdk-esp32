#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int audio_visualizer_init(void);

int audio_visualizer_processing(uint8_t *audio_data, uint32_t data_size);

int audio_visualizer_deinit(void);

#ifdef __cplusplus
}
#endif