
#pragma once

#include "esp_peer.h"
#include "esp_peer_signaling.h"
#include "esp_capture.h"
#include "av_render.h"

#include "common.h"
#include "protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief  Handle to an engine instance.
typedef void *engine_handle_t;

typedef enum {
    ENGINE_ERR_NONE        =  0,
    ENGINE_ERR_INVALID_ARG = -1,
    ENGINE_ERR_NO_MEM      = -2,
    ENGINE_ERR_SIGNALING   = -3,
    ENGINE_ERR_RTC         = -4,
    ENGINE_ERR_MEDIA       = -5,
    ENGINE_ERR_OTHER       = -6,
    // TODO: Add more error cases as needed
} engine_err_t;

/// @brief  WebRTC media provider
/// @note   Media player and capture system are created externally.
///         WebRTC will internally use the capture and player handle to capture media data and perform media playback.
typedef struct {
    esp_capture_handle_t capture; /*!< Capture system handle */
    av_render_handle_t   player;  /*!< Player handle */
} engine_media_provider_t;

typedef struct {
    // This is an alternative to RtcEngine's async connect method
    livekit_pb_join_response_t join_response;
} engine_event_connected_t;

typedef struct {
    livekit_pb_disconnect_reason_t reason;
} engine_event_disconnected_t;

typedef struct {
    // This is an alternative to RtcEngine's async connect method
    // returning a error result.
    // TODO: add error details
} engine_event_error_t;

typedef struct {
    void *ctx;

    void (*on_connected)(engine_event_connected_t detail, void *ctx);
    void (*on_disconnected)(engine_event_disconnected_t detail, void *ctx);
    void (*on_error)(engine_event_error_t detail, void *ctx);
    void (*on_data_packet)(livekit_pb_data_packet_t* packet, void *ctx);
    engine_media_options_t media;
} engine_options_t;

/// @brief Creates a new instance.
/// @param[out] handle The handle to the new instance.
engine_err_t engine_create(engine_handle_t *handle, engine_options_t *options);

/// @brief Destroys an instance.
/// @param[in] handle The handle to the instance to destroy.
engine_err_t engine_destroy(engine_handle_t handle);

/// @brief Connect the engine.
engine_err_t engine_connect(engine_handle_t handle, const char* server_url, const char* token);

/// @brief Close the engine.
engine_err_t engine_close(engine_handle_t handle);

/// @brief Sends a data packet to the remote peer.
engine_err_t engine_send_data_packet(engine_handle_t handle, const livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind);

#ifdef __cplusplus
}
#endif
