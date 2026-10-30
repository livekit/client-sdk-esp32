/*
 * Copyright 2026 Espressif Systems (Shanghai) CO LTD
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * DVP wiring and initialization derived from Espressif's ESP32-S31-Korvo BSP
 * at esp-dev-kits commit df877cb1124a80835ad22fa5d8bafadb2348ce50.
 */

// Connect the Korvo onboard DVP camera to esp_video and correct its orientation
// through device controls so capture users do not need board-specific setup.

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S31

#include <fcntl.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "driver/gpio.h"
#include "esp_cam_ctlr_dvp.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_init.h"
#include "esp_video_ioctl.h"

#include "s31_korvo_audio.h"
#include "s31_korvo_camera.h"

// Pin mapping follows the Korvo BSP revision cited above.
#define S31_KORVO_CAMERA_D0        GPIO_NUM_46
#define S31_KORVO_CAMERA_D1        GPIO_NUM_47
#define S31_KORVO_CAMERA_D2        GPIO_NUM_48
#define S31_KORVO_CAMERA_D3        GPIO_NUM_49
#define S31_KORVO_CAMERA_D4        GPIO_NUM_50
#define S31_KORVO_CAMERA_D5        GPIO_NUM_51
#define S31_KORVO_CAMERA_D6        GPIO_NUM_52
#define S31_KORVO_CAMERA_D7        GPIO_NUM_53
#define S31_KORVO_CAMERA_PCLK      GPIO_NUM_54
#define S31_KORVO_CAMERA_XCLK      GPIO_NUM_55
#define S31_KORVO_CAMERA_VSYNC     GPIO_NUM_56
#define S31_KORVO_CAMERA_HSYNC     GPIO_NUM_57
#define S31_KORVO_CAMERA_XCLK_HZ   (20 * 1000 * 1000)
#define S31_KORVO_CAMERA_SCCB_HZ   (10 * 1000)

static const char *TAG = "s31_korvo_camera";
static bool s_camera_initialized;

esp_err_t s31_korvo_camera_init(void)
{
    if (s_camera_initialized) {
        return ESP_OK;
    }

    // Audio owns the shared control bus; reuse it rather than install a second
    // I2C driver for the camera's SCCB transactions.
    i2c_master_bus_handle_t i2c_bus =
        s31_korvo_audio_get_i2c_bus_handle();
    ESP_RETURN_ON_FALSE(i2c_bus != NULL, ESP_ERR_INVALID_STATE, TAG,
                        "initialize Korvo audio before the shared camera bus");

    esp_cam_ctlr_dvp_pin_config_t pins = {
        .data_width = 8,
        .data_io = {
            S31_KORVO_CAMERA_D0,
            S31_KORVO_CAMERA_D1,
            S31_KORVO_CAMERA_D2,
            S31_KORVO_CAMERA_D3,
            S31_KORVO_CAMERA_D4,
            S31_KORVO_CAMERA_D5,
            S31_KORVO_CAMERA_D6,
            S31_KORVO_CAMERA_D7,
        },
        .vsync_io = S31_KORVO_CAMERA_VSYNC,
        .de_io = S31_KORVO_CAMERA_HSYNC,
        .pclk_io = S31_KORVO_CAMERA_PCLK,
        .xclk_io = S31_KORVO_CAMERA_XCLK,
    };
    esp_video_init_dvp_config_t dvp_config = {
        .sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus,
            .freq = S31_KORVO_CAMERA_SCCB_HZ,
        },
        .reset_pin = GPIO_NUM_NC,
        .pwdn_pin = GPIO_NUM_NC,
        .dvp_pin = pins,
        .xclk_freq = S31_KORVO_CAMERA_XCLK_HZ,
    };
    esp_video_init_config_t video_config = {
        .dvp = &dvp_config,
    };

    ESP_LOGI(TAG, "Initializing Korvo-1 DVP camera as %s",
             ESP_VIDEO_DVP_DEVICE_NAME);
    ESP_RETURN_ON_ERROR(
        esp_video_init_with_flags(&video_config, ESP_VIDEO_INIT_FLAGS_DVP),
        TAG, "DVP camera initialization failed");
    s_camera_initialized = true;
    return ESP_OK;
}

esp_err_t s31_korvo_camera_apply_orientation(void)
{
    ESP_RETURN_ON_FALSE(s_camera_initialized, ESP_ERR_INVALID_STATE, TAG,
                        "initialize the camera before applying orientation");

    int fd = open(ESP_VIDEO_DVP_DEVICE_NAME, O_RDONLY);
    ESP_RETURN_ON_FALSE(fd >= 0, ESP_FAIL, TAG,
                        "failed to open the DVP camera controls");

    // Both flips compensate for the onboard sensor's mounting orientation
    // without adding a rotation stage to the video capture pipeline.
    struct v4l2_ext_control controls_data[] = {
        { .id = V4L2_CID_VFLIP, .value = 1 },
        { .id = V4L2_CID_HFLIP, .value = 1 },
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_USER,
        .count = sizeof(controls_data) / sizeof(controls_data[0]),
        .controls = controls_data,
    };
    int result = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
    close(fd);
    ESP_RETURN_ON_FALSE(result == 0, ESP_FAIL, TAG,
                        "failed to rotate the onboard camera image");

    ESP_LOGI(TAG, "Applied Korvo-1 camera rotation (vertical + horizontal flip)");
    return ESP_OK;
}

#endif // CONFIG_IDF_TARGET_ESP32S31
