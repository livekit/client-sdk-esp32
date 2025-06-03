
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void *livekit_peer_handle_t;

typedef enum {
    LIVEKIT_PEER_ERR_NONE        =  0,
    LIVEKIT_PEER_ERR_INVALID_ARG = -1,
    LIVEKIT_PEER_ERR_NO_MEM      = -2,
    LIVEKIT_PEER_ERR_RTC         = -3
} livekit_peer_err_t;

typedef enum {
    LIVEKIT_PEER_KIND_NONE,
    LIVEKIT_PEER_KIND_SUBSCRIBER,
    LIVEKIT_PEER_KIND_PUBLISHER,
} livekit_peer_kind_t;

livekit_peer_err_t livekit_peer_create(livekit_peer_kind_t kind, livekit_peer_handle_t *handle);
livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle);

livekit_peer_err_t livekit_peer_connect(esp_peer_ice_server_cfg_t *server_info, int server_num, livekit_peer_handle_t handle);
livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle);

#ifdef __cplusplus
}
#endif