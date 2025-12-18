#include <math.h>
#include "esp_log.h"
#include "miniscale.h"
#include "scale.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "scale";

#define LED_COLOR_IDLE 0x0000FF
#define LED_COLOR_MEASURING 0x00FF00

#define OBJECT_DETECT_THRESHOLD_GRAMS 10
#define STABLE_TOLERANCE_GRAMS 1
#define STABLE_COUNT_REQUIRED 3

#define SAMPLE_INTERVAL_MS 100
#define READ_TIMEOUT_MS 10000

static miniscale_handle_t scale_handle;

esp_err_t scale_init(i2c_master_bus_handle_t bus)
{
    if (scale_handle != NULL) {
        return ESP_OK;
    }
    scale_handle = miniscale_init(bus);
    if (scale_handle == NULL) {
        ESP_LOGW(TAG, "Unable to initialize scale");
        return ESP_FAIL;
    }

    // Write configuration
    miniscale_set_lp_filter(scale_handle, true);
    miniscale_set_avg_filter(scale_handle, 10);
    miniscale_set_ema_filter(scale_handle, 10);
    miniscale_set_offset(scale_handle);
    miniscale_set_led_color(scale_handle, LED_COLOR_IDLE);

    return ESP_OK;
}

bool scale_read(float* weight_grams)
{
    bool success = false;
    float reading = 0, prev_reading = 0;
    int stable_readings = 0;
    int elapsed_ms = 0;

    miniscale_set_offset(scale_handle);
    miniscale_set_led_color(scale_handle, LED_COLOR_MEASURING);

    while (true) {
        if (elapsed_ms >= READ_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Read timeout");
            break;
        }
        elapsed_ms += SAMPLE_INTERVAL_MS;
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));

        prev_reading = reading;
        if (miniscale_get_weight(scale_handle, &reading) != ESP_OK) {
            ESP_LOGE(TAG, "Read failed");
            break;
        }
        if (reading < OBJECT_DETECT_THRESHOLD_GRAMS) {
            stable_readings = 0;
            continue;
        }
        if (fabsf(reading - prev_reading) > STABLE_TOLERANCE_GRAMS) {
            stable_readings = 0;
            continue;
        }
        stable_readings += 1;
        if (stable_readings >= STABLE_COUNT_REQUIRED) {
            ESP_LOGI(TAG, "Object weight: %fg", reading);
            *weight_grams = reading;
            success = true;
            break;
        }
    }

    miniscale_set_led_color(scale_handle, LED_COLOR_IDLE);
    return success;
}

bool scale_is_available(void)
{
    return scale_handle != NULL;
}