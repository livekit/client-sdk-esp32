#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int audio_visualizer_process(uint8_t *audio_data, uint32_t data_size);


#ifdef __cplusplus
}
#endif