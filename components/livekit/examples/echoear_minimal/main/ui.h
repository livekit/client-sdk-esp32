
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_acquire(void);
void ui_release(void);

#ifdef __cplusplus
}
#endif