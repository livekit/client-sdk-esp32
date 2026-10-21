#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "miniscale.h"

#define MINISCALE_DEFAULT_ADDR 0x26
#define MINISCALE_I2C_TIMEOUT_MS 1000
#define MINISCALE_MAX_DATA_LEN 16

#define MINISCALE_REG_RAW_ADC          0x00
#define MINISCALE_REG_CAL_DATA         0x10
#define MINISCALE_REG_BUTTON           0x20
#define MINISCALE_REG_RGB_LED          0x30
#define MINISCALE_REG_SET_GAP          0x40
#define MINISCALE_REG_SET_OFFSET       0x50
#define MINISCALE_REG_CAL_DATA_INT     0x60
#define MINISCALE_REG_CAL_DATA_STRING  0x70
#define MINISCALE_REG_FILTER           0x80
#define MINISCALE_REG_JUMP_BOOTLOADER  0xFD
#define MINISCALE_REG_FIRMWARE_VERSION 0xFE
#define MINISCALE_REG_I2C_ADDRESS      0xFF

#define MINISCALE_FILTER_LP_OFFSET     0
#define MINISCALE_FILTER_AVG_OFFSET    1
#define MINISCALE_FILTER_EMA_OFFSET    2

typedef struct {
    i2c_master_dev_handle_t i2c_dev;  ///< I2C device handle
} miniscale_t;

/// Write bytes to device register
static esp_err_t miniscale_write_bytes(miniscale_handle_t handle, uint8_t reg, const uint8_t *data, size_t len)
{
    if (!handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > MINISCALE_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    miniscale_t *dev = (miniscale_t *)handle;

    uint8_t write_buf[MINISCALE_MAX_DATA_LEN + 1];
    write_buf[0] = reg;
    if (len > 0) {
        memcpy(&write_buf[1], data, len);
    }
    return i2c_master_transmit(dev->i2c_dev, write_buf, len + 1, MINISCALE_I2C_TIMEOUT_MS);
}

/// Read bytes from device register
static esp_err_t miniscale_read_bytes(miniscale_handle_t handle, uint8_t reg, uint8_t *data, size_t len)
{
    if (!handle) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > MINISCALE_MAX_DATA_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    miniscale_t *dev = (miniscale_t *)handle;
    esp_err_t ret = i2c_master_transmit(dev->i2c_dev, &reg, 1, MINISCALE_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    return i2c_master_receive(dev->i2c_dev, data, len, MINISCALE_I2C_TIMEOUT_MS);
}

// MARK: - Public API

miniscale_handle_t miniscale_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        return NULL;
    }
    miniscale_t *sensor = (miniscale_t *)calloc(1, sizeof(miniscale_t));
    if (!sensor) {
        return NULL;
    }
    i2c_device_config_t dev_cfg = {
        .device_address = MINISCALE_DEFAULT_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &sensor->i2c_dev) != ESP_OK) {
        free(sensor);
        return NULL;
    }
    return (miniscale_handle_t)sensor;
}

esp_err_t miniscale_deinit(miniscale_handle_t *handle)
{
    if (*handle == NULL) {
        return ESP_OK;
    }
    miniscale_t *sensor = (miniscale_t *)(*handle);
    i2c_master_bus_rm_device(sensor->i2c_dev);
    free(sensor);
    *handle = NULL;
    return ESP_OK;
}

esp_err_t miniscale_get_raw_adc(miniscale_handle_t handle, int32_t *raw_adc)
{
    if (!handle || !raw_adc) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[4] = {0};
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_RAW_ADC, data, 4);
    if (ret != ESP_OK) {
        return ret;
    }
    *raw_adc = (int32_t)((uint32_t)data[0] |
                        ((uint32_t)data[1] << 8) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24));
    return ESP_OK;
}

esp_err_t miniscale_get_weight(miniscale_handle_t handle, float *weight)
{
    if (!handle || !weight) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[4];
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_CAL_DATA, data, 4);
    if (ret != ESP_OK) {
        return ret;
    }
    memcpy(weight, data, 4);
    return ESP_OK;
}

esp_err_t miniscale_get_gap_value(miniscale_handle_t handle, float *gap_value)
{
    if (!handle || !gap_value) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[4];
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_SET_GAP, data, 4);
    if (ret != ESP_OK) {
        return ret;
    }
    memcpy(gap_value, data, 4);
    return ESP_OK;
}

esp_err_t miniscale_set_gap_value(miniscale_handle_t handle, float gap_value)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[4];
    memcpy(data, &gap_value, 4);
    esp_err_t ret = miniscale_write_bytes(handle, MINISCALE_REG_SET_GAP, data, 4);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t miniscale_set_offset(miniscale_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data = 1;
    return miniscale_write_bytes(handle, MINISCALE_REG_SET_OFFSET, &data, 1);
}

esp_err_t miniscale_set_led_color(miniscale_handle_t handle, uint32_t color)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[3];
    data[0] = (color >> 16) & 0xFF;  // Red
    data[1] = (color >> 8) & 0xFF;   // Green
    data[2] = color & 0xFF;          // Blue
    return miniscale_write_bytes(handle, MINISCALE_REG_RGB_LED, data, 3);
}

esp_err_t miniscale_get_led_color(miniscale_handle_t handle, uint32_t *color)
{
    if (!handle || !color) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data[3];
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_RGB_LED, data, 3);
    if (ret != ESP_OK) {
        return ret;
    }
    *color = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    return ESP_OK;
}

esp_err_t miniscale_get_button_status(miniscale_handle_t handle, bool *button_status)
{
    if (!handle || !button_status) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_BUTTON, (uint8_t *)button_status, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    *button_status = !*button_status;
    return ESP_OK;
}

esp_err_t miniscale_get_firmware_version(miniscale_handle_t handle, uint8_t *version)
{
    if (!handle || !version) {
        return ESP_ERR_INVALID_ARG;
    }
    return miniscale_read_bytes(handle, MINISCALE_REG_FIRMWARE_VERSION, version, 1);
}

esp_err_t miniscale_jump_bootloader(miniscale_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data = 1;
    return miniscale_write_bytes(handle, MINISCALE_REG_JUMP_BOOTLOADER, &data, 1);
}

esp_err_t miniscale_set_lp_filter(miniscale_handle_t handle, bool enable)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data = enable ? 1 : 0;
    return miniscale_write_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_LP_OFFSET, &data, 1);
}

esp_err_t miniscale_get_lp_filter(miniscale_handle_t handle, bool *enabled)
{
    if (!handle || !enabled) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t data;
    esp_err_t ret = miniscale_read_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_LP_OFFSET, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    *enabled = (data != 0);
    return ESP_OK;
}

esp_err_t miniscale_set_avg_filter(miniscale_handle_t handle, uint8_t avg_value)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return miniscale_write_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_AVG_OFFSET, &avg_value, 1);
}

esp_err_t miniscale_get_avg_filter(miniscale_handle_t handle, uint8_t *avg_value)
{
    if (!handle || !avg_value) {
        return ESP_ERR_INVALID_ARG;
    }
    return miniscale_read_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_AVG_OFFSET, avg_value, 1);
}

esp_err_t miniscale_set_ema_filter(miniscale_handle_t handle, uint8_t ema_value)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return miniscale_write_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_EMA_OFFSET, &ema_value, 1);
}

esp_err_t miniscale_get_ema_filter(miniscale_handle_t handle, uint8_t *ema_value)
{
    if (!handle || !ema_value) {
        return ESP_ERR_INVALID_ARG;
    }
    return miniscale_read_bytes(handle, MINISCALE_REG_FILTER + MINISCALE_FILTER_EMA_OFFSET, ema_value, 1);
}
