#include "esp_log.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "livekit.h"
#include "network.h"
#include "media.h"
#include "ui.h"
#include "board.h"
#include "example.h"

extern lv_subject_t ui_is_network_connected;

static int network_event_handler(bool connected)
{
    lv_subject_set_int(&ui_is_network_connected, (int)connected);
    return 0;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI("main", "** [1]");
    livekit_system_init();
    board_init();
    media_init();
    ui_init();
    example_init();
    network_init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD, network_event_handler);
}
