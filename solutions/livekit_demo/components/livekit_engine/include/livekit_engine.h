
#pragma once

#include "esp_peer.h"
#include "esp_peer_signaling.h"
#include "esp_capture.h"
#include "av_render.h"

#include "livekit_common.h"
#include "livekit_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief  Handle to an engine instance.
typedef void *livekit_eng_handle_t;

typedef enum {
    LIVEKIT_ENG_ERR_NONE        =  0,
    LIVEKIT_ENG_ERR_INVALID_ARG = -1,
    LIVEKIT_ENG_ERR_NO_MEM      = -2,
    LIVEKIT_ENG_ERR_SIGNALING   = -3,
    LIVEKIT_ENG_ERR_RTC         = -4,
    LIVEKIT_ENG_ERR_OTHER       = -5,
    // TODO: Add more error cases as needed
} livekit_eng_err_t;

/// @brief  WebRTC media provider
/// @note   Media player and capture system are created externally.
///         WebRTC will internally use the capture and player handle to capture media data and perform media playback.
typedef struct {
    esp_capture_handle_t capture; /*!< Capture system handle */
    av_render_handle_t   player;  /*!< Player handle */
} livekit_eng_media_provider_t;

typedef struct {
    // This is an alternative to RtcEngine's async connect method
    livekit_pb_join_response_t join_response;
} livekit_eng_event_connected_t;

typedef struct {
    livekit_pb_disconnect_reason_t reason;
} livekit_eng_event_disconnected_t;

typedef struct {
    // This is an alternative to RtcEngine's async connect method
    // returning a error result.
    // TODO: add error details
} livekit_eng_event_error_t;

typedef struct {
    void *ctx;

    void (*on_connected)(livekit_eng_event_connected_t detail, void *ctx);
    void (*on_disconnected)(livekit_eng_event_disconnected_t detail, void *ctx);
    void (*on_error)(livekit_eng_event_error_t detail, void *ctx);

    void (*on_user_packet)(livekit_pb_user_packet_t* packet, void *ctx);
    void (*on_rpc_request)(livekit_pb_rpc_request_t* req, void *ctx);
    void (*on_rpc_response)(livekit_pb_rpc_response_t* res, void *ctx);
    void (*on_rpc_ack)(livekit_pb_rpc_ack_t* ack, void *ctx);
    void (*on_stream_header)(livekit_pb_data_stream_header_t* header, void *ctx);
    void (*on_stream_chunk)(livekit_pb_data_stream_chunk_t* chunk, void *ctx);
    void (*on_stream_trailer)(livekit_pb_data_stream_trailer_t* trailer, void *ctx);

    livekit_eng_media_options_t media;
} livekit_eng_options_t;

/// @brief Creates a new instance.
/// @param[out] handle The handle to the new instance.
livekit_eng_err_t livekit_eng_create(livekit_eng_handle_t *handle, livekit_eng_options_t *options);

/// @brief Destroys an instance.
/// @param[in] handle The handle to the instance to destroy.
livekit_eng_err_t livekit_eng_destroy(livekit_eng_handle_t handle);

/// @brief Connect the engine.
livekit_eng_err_t livekit_eng_connect(livekit_eng_handle_t handle, const char* server_url, const char* token);

/// @brief Close the engine.
livekit_eng_err_t livekit_eng_close(livekit_eng_handle_t handle);

/// @brief Sends a data packet to the remote peer.
livekit_eng_err_t livekit_eng_send_data_packet(livekit_eng_handle_t handle, livekit_pb_data_packet_t* packet, livekit_pb_data_packet_kind_t kind);

#ifdef __cplusplus
}
#endif
