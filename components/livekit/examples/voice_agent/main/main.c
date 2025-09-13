#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "network_connect.h"
#include "board.h"
#include "example.h"
#include "media.h"

#include "livekit.h"

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    livekit_system_init();
    board_init();
    media_init();
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
        ESP_SNTP_SERVER_LIST("time.google.com", "pool.ntp.org"));
    esp_netif_sntp_init(&sntp_config);

    if (network_connect()) {
        join_room(); // See example.c
    }
}
