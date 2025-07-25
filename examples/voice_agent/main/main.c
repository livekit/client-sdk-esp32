#include "esp_log.h"
#include "sdkconfig.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "livekit.h"
#include "network.h"
#include "media.h"
#include "board.h"
#include "example.h"

static const char *TAG = "main";

static void run_async_join_room(void *arg)
{
    join_room(); // See example.c
    media_lib_thread_destroy(NULL);
}

static int network_event_handler(bool connected)
{
    // Create and join the room once network is connected.
    if (!connected) return 0;
    media_lib_thread_create_from_scheduler(NULL, "join", run_async_join_room, NULL);
    return 0;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 Box-3 Voice Agent Starting ===");
    
    ESP_LOGI(TAG, "Setting log level...");
    esp_log_level_set("*", ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Initializing LiveKit system...");
    livekit_system_init();
    ESP_LOGI(TAG, "LiveKit system initialized");
    
    ESP_LOGI(TAG, "Initializing board...");
    board_init();
    ESP_LOGI(TAG, "Board initialized");
    
    ESP_LOGI(TAG, "Initializing media...");
    media_init();
    ESP_LOGI(TAG, "Media initialized");
    
    ESP_LOGI(TAG, "Initializing network...");
    network_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, network_event_handler);
    ESP_LOGI(TAG, "Network initialized");
    
    ESP_LOGI(TAG, "=== Voice Agent initialization complete ===");
}
