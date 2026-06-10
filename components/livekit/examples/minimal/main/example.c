#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_system.h"
#include "esp_log.h"
#include "livekit.h"
#include "livekit_sandbox.h"
#include "media.h"
#include "board.h"
#include "example.h"

static const char *TAG = "livekit_example";

static livekit_room_handle_t room_handle;

/// Invoked when the room's connection state changes.
static void on_state_changed(livekit_connection_state_t state, void* ctx)
{
    ESP_LOGI(TAG, "Room state changed: %s", livekit_connection_state_str(state));

    livekit_failure_reason_t reason = livekit_room_get_failure_reason(room_handle);
    if (reason != LIVEKIT_FAILURE_REASON_NONE) {
        ESP_LOGE(TAG, "Failure reason: %s", livekit_failure_reason_str(reason));
    }
}

void create_room()
{
    if (room_handle != NULL) {
        ESP_LOGE(TAG, "Room already created");
        return;
    }

    livekit_room_options_t room_options = {
        .publish = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .audio_encode = {
                .codec = LIVEKIT_AUDIO_CODEC_OPUS,
                .sample_rate = 16000,
                .channel_count = 1
            },
            .capturer = media_get_capturer()
        },
        .subscribe = {
            .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
            .renderer = media_get_renderer()
        },
        .on_state_changed = on_state_changed
    };
    if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to create room");
    }
}

void connect_room()
{
    // Create the room on demand so connecting after a destroy recreates it.
    if (room_handle == NULL) {
        create_room();
    }
    if (room_handle == NULL) {
        return;
    }

    livekit_err_t connect_res;
#ifdef CONFIG_LK_EXAMPLE_USE_SANDBOX
    // Option A: Sandbox token server.
    livekit_sandbox_res_t res = {};
    livekit_sandbox_options_t gen_options = {
        .sandbox_id = CONFIG_LK_EXAMPLE_SANDBOX_ID,
        .room_name = CONFIG_LK_EXAMPLE_ROOM_NAME,
        .participant_name = CONFIG_LK_EXAMPLE_PARTICIPANT_NAME
    };
    if (!livekit_sandbox_generate(&gen_options, &res)) {
        ESP_LOGE(TAG, "Failed to generate sandbox token");
        return;
    }
    connect_res = livekit_room_connect(room_handle, res.server_url, res.token);
    livekit_sandbox_res_free(&res);
#else
    // Option B: Pre-generated token.
    connect_res = livekit_room_connect(
        room_handle,
        CONFIG_LK_EXAMPLE_SERVER_URL,
        CONFIG_LK_EXAMPLE_TOKEN);
#endif

    if (connect_res != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to connect to room");
    }
}

void disconnect_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return;
    }
    if (livekit_room_close(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to disconnect from room");
    }
}

void destroy_room()
{
    if (room_handle == NULL) {
        ESP_LOGE(TAG, "Room not created");
        return;
    }
    if (livekit_room_destroy(room_handle) != LIVEKIT_ERR_NONE) {
        ESP_LOGE(TAG, "Failed to destroy room");
        return;
    }
    room_handle = NULL;
}

/// Reads single keypresses from the console to drive the room lifecycle.
///
/// Press 'c' to connect, 'd' to disconnect, and 'r' to destroy the room.
/// Connecting recreates the room if it was destroyed, so this exercises the
/// full connect / disconnect / destroy / reconnect cycle on demand.
///
/// Reads directly from the USB-Serial-JTAG peripheral so input works when the
/// board is connected via its native USB port (the default secondary console).
///
static void serial_control_task(void *arg)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Serial JTAG driver");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Press 'c' to connect, 'd' to disconnect, 'r' to destroy");
    uint8_t ch;
    while (true) {
        if (usb_serial_jtag_read_bytes(&ch, 1, portMAX_DELAY) <= 0) {
            continue;
        }
        switch (ch) {
            case 'c':
                ESP_LOGW(TAG, "Connecting (free heap: %" PRIu32 ")", esp_get_free_heap_size());
                connect_room();
                break;
            case 'd':
                ESP_LOGW(TAG, "Disconnecting (free heap: %" PRIu32 ")", esp_get_free_heap_size());
                disconnect_room();
                break;
            case 'r':
                ESP_LOGW(TAG, "Destroying room (free heap before: %" PRIu32 ")", esp_get_free_heap_size());
                destroy_room();
                ESP_LOGW(TAG, "Room destroyed (free heap after: %" PRIu32 ")", esp_get_free_heap_size());
                break;
            default:
                break;
        }
    }
}

void start_serial_control()
{
    xTaskCreate(serial_control_task, "serial_ctrl", 6144, NULL, 5, NULL);
}