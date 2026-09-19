/// @file miniscale.h
/// I2C driver for M5Stack Unit Mini Scale module

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/// MiniScale device handle (opaque type)
typedef void* miniscale_handle_t;

/// Initialize MiniScale device
///
/// @param bus I2C bus handle
/// @return miniscale_handle_t Handle on success, NULL on failure
///
miniscale_handle_t miniscale_init(i2c_master_bus_handle_t bus);

/// Deinitialize MiniScale device
///
/// @param handle Pointer to device handle to deinitialize
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_deinit(miniscale_handle_t *handle);

/// Read raw ADC value from the sensor
///
/// @param handle Device handle
/// @param raw_adc Pointer to store the raw ADC value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_raw_adc(miniscale_handle_t handle, int32_t *raw_adc);

/// Read calibrated weight value as float
///
/// @param handle Device handle
/// @param weight Pointer to store the weight value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_weight(miniscale_handle_t handle, float *weight);

/// Get gap value for calibration
///
/// @param handle Device handle
/// @param gap_value Pointer to store the gap value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_gap_value(miniscale_handle_t handle, float *gap_value);

/// Set gap value for calibration
///
/// @param handle Device handle
/// @param gap_value Gap value to set
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_gap_value(miniscale_handle_t handle, float gap_value);

/// Set offset for calibration (tare)
///
/// @param handle Device handle
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_offset(miniscale_handle_t handle);

/// Set RGB LED color
///
/// @param handle Device handle
/// @param color RGB color value (0xRRGGBB format)
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_led_color(miniscale_handle_t handle, uint32_t color);

/// Get current RGB LED color
///
/// @param handle Device handle
/// @param color Pointer to store the color value (0xRRGGBB format)
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_led_color(miniscale_handle_t handle, uint32_t *color);

/// Get button status
///
/// @param handle Device handle
/// @param button_status Pointer to store the button status
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_button_status(miniscale_handle_t handle, bool *button_status);

/// Get firmware version
///
/// @param handle Device handle
/// @param version Pointer to store the firmware version
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_firmware_version(miniscale_handle_t handle, uint8_t *version);

/// Jump to bootloader mode
///
/// @param handle Device handle
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_jump_bootloader(miniscale_handle_t handle);

/// Set low-pass filter enable/disable
///
/// @param handle Device handle
/// @param enable True to enable, false to disable
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_lp_filter(miniscale_handle_t handle, bool enable);

/// Get low-pass filter status
///
/// @param handle Device handle
/// @param enabled Pointer to store the filter status
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_lp_filter(miniscale_handle_t handle, bool *enabled);

/// Set average filter value
///
/// @param handle Device handle
/// @param avg_value Average filter value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_avg_filter(miniscale_handle_t handle, uint8_t avg_value);

/// Get average filter value
///
/// @param handle Device handle
/// @param avg_value Pointer to store the average filter value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_avg_filter(miniscale_handle_t handle, uint8_t *avg_value);

/// Set EMA filter value
///
/// @param handle Device handle
/// @param ema_value EMA filter value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_set_ema_filter(miniscale_handle_t handle, uint8_t ema_value);

/// Get EMA filter value
///
/// @param handle Device handle
/// @param ema_value Pointer to store the EMA filter value
/// @return esp_err_t ESP_OK on success, error code on failure
///
esp_err_t miniscale_get_ema_filter(miniscale_handle_t handle, uint8_t *ema_value);

#ifdef __cplusplus
}
#endif