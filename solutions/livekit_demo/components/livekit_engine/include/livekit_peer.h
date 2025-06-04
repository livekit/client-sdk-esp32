
#pragma once

#include "livekit_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *livekit_peer_handle_t;

typedef enum {
    LIVEKIT_PEER_ERR_NONE           =  0,
    LIVEKIT_PEER_ERR_INVALID_ARG    = -1,
    LIVEKIT_PEER_ERR_NO_MEM         = -2,
    LIVEKIT_PEER_ERR_INVALID_STATE  = -3,
    LIVEKIT_PEER_ERR_RTC            = -4
} livekit_peer_err_t;

typedef enum {
    LIVEKIT_PEER_KIND_NONE,
    LIVEKIT_PEER_KIND_SUBSCRIBER,
    LIVEKIT_PEER_KIND_PUBLISHER,
} livekit_peer_kind_t;

livekit_peer_err_t livekit_peer_create(livekit_peer_kind_t kind, livekit_peer_handle_t *handle);
livekit_peer_err_t livekit_peer_destroy(livekit_peer_handle_t handle);

livekit_peer_err_t livekit_peer_connect(livekit_peer_handle_t handle);
livekit_peer_err_t livekit_peer_disconnect(livekit_peer_handle_t handle);

/// @brief Sets the ICE server to use for the connection
/// @note Must be called prior to establishing the connection.
livekit_peer_err_t livekit_peer_set_ice_servers(livekit_ice_server_t *servers, int count, livekit_peer_handle_t handle);

#ifdef __cplusplus
}
#endif