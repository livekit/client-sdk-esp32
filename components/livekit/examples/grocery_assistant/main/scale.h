#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the scale.
esp_err_t scale_init(i2c_master_bus_handle_t bus);

/// Read the weight of an object on the scale, waiting for an object to be
/// placed and the reading to become stable.
///
/// @param weight_grams Pointer to store the weight value
/// @return true if the weight is read successfully, false otherwise
///
bool scale_read(float* weight_grams);

/// Check if the scale is available.
bool scale_is_available(void);

#ifdef __cplusplus
}
#endif
