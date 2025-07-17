#include "esp_log.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "livekit.h"
#include "network.h"
#include "media.h"
#include "board.h"
#include "example.h"

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
    esp_log_level_set("*", ESP_LOG_INFO);
    livekit_system_init();
    board_init();
    media_init();
    network_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, network_event_handler);
}
