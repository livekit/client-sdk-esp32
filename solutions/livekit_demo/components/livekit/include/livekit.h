
#pragma once

#include "esp_capture.h"
#include "av_render.h"
#include "livekit_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Handle to a room object.
typedef void *livekit_room_handle_t;

/// @brief Result
typedef enum {
    LIVEKIT_ERR_NONE          =  0,
    LIVEKIT_ERR_INVALID_ARG   = -1,
    LIVEKIT_ERR_NO_MEM        = -2,
    LIVEKIT_ERR_ENGINE        = -3,
    LIVEKIT_ERR_OTHER         = -4,
    LIVEKIT_ERR_INVALID_STATE = -5,
    // TODO: Add more error cases as needed
} livekit_err_t;

/// @brief Video codec to use within a room.
typedef enum {
    LIVEKIT_VIDEO_CODEC_NONE = 0, // No video codec set
    LIVEKIT_VIDEO_CODEC_H264 = 1  // H.264 (AVC)
} livekit_video_codec_t;

/// @brief Audio codec to use within a room.
typedef enum {
    LIVEKIT_AUDIO_CODEC_NONE  = 0, // No audio codec set
    LIVEKIT_AUDIO_CODEC_G711A = 1, // G.711 A-law (PCMA)
    LIVEKIT_AUDIO_CODEC_G711U = 2, // G.711 u-law (PCMU)
    LIVEKIT_AUDIO_CODEC_OPUS  = 3  // Opus
} livekit_audio_codec_t;

/// @brief Media mode for the room.
typedef enum {
    LIVEKIT_MEDIA_TYPE_NONE = 0,         // No media
    LIVEKIT_MEDIA_TYPE_AUDIO = (1 << 0), // Audio only
    LIVEKIT_MEDIA_TYPE_VIDEO = (1 << 1), // Video only
    LIVEKIT_MEDIA_TYPE_BOTH  = LIVEKIT_MEDIA_TYPE_AUDIO | LIVEKIT_MEDIA_TYPE_VIDEO, // Audio and video
} livekit_media_kind_t;

/// @brief Options for the video encoder.
typedef struct {
    livekit_video_codec_t codec;  // Codec to use for encoding
    int width;                    // Output frame width in pixels
    int height;                   // Output frame height in pixels
    int fps;                      // Output frame per second
} livekit_video_encode_options_t;

/// @brief Options for the audio encoder.
typedef struct {
    livekit_audio_codec_t codec;  // Codec to use for encoding
    uint32_t sample_rate;         // Output sample rate in Hz
    uint8_t channel_count;        // Output number of channels
} livekit_audio_encode_options_t;

/// @brief Options for publishing a data packet.
typedef struct {
    /// @brief Topic to send the data packet under.
    char* topic;

    /// @brief Whether the data packet is sent using the lossy channel.
    bool lossy;

    /// @brief Identifies of participants to send the data packet to. If not
    /// specified, the data packet is sent to all participants.
    char** destination_identities;

    /// @brief Number of destination identities.
    int destination_identities_count;
} livekit_publish_data_options_t;

/// @brief Payload containing a pointer to data and its size.
typedef struct {
    uint8_t *bytes;
    size_t size;
} livekit_payload_t;

/// @brief Options for publishing media.
typedef struct {
    /// @brief Kind of media that can be published.
    livekit_media_kind_t kind;

    /// @brief Video encoder options.
    /// @note Only required if the room publishes video.
    livekit_video_encode_options_t video_encode;

    /// @brief Audio encoder options.
    /// @note Only required if the room publishes audio.
    livekit_audio_encode_options_t audio_encode;

    /// @brief Capturer to use for obtaining media to publish.
    /// @note Only required if the room publishes media.
    esp_capture_handle_t capturer;
} livekit_pub_options_t;

/// @brief Options for subscribing to media.
typedef struct {
    /// @brief Kind of media that can be subscribed to.
    livekit_media_kind_t kind;

    /// @brief Renderer to use for subscribed media tracks.
    /// @note Only required if the room subscribes to media.
    av_render_handle_t renderer;
} livekit_sub_options_t;

/// @brief Options for a room.
typedef struct {
    /// @brief Options for publishing media.
    /// @note Only required if the room publishes media.
    livekit_pub_options_t publish;

    /// @brief Options for subscribing to media.
    /// @note Only required if the room subscribes to media.
    livekit_sub_options_t subscribe;

    void (*on_rpc_result)(const livekit_rpc_result_t* result, void* ctx);

    /// @brief User context passed to all handlers.
    void* ctx;
} livekit_room_options_t;

/// @brief Creates a room.
/// @param handle[out] Room handle.
/// @param options[in] Options for the new room.
/// @return LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_create(livekit_room_handle_t *handle, const livekit_room_options_t *options);

/// @brief Destroys a room.
/// @param handle[in] Room handle.
/// @return LIVEKIT_ERR_NONE if successful, otherwise an error code.
/// @warning Be sure to close the room before destroying it for normal closure.
livekit_err_t livekit_room_destroy(livekit_room_handle_t handle);

/// @brief Connects to a room asynchronously.
/// @param handle[in] Room handle.
/// @param server_url[in] URL of the LiveKit server beginning with "wss://" or "ws://" (development only).
/// @param token[in] Server-generated token for authentication.
/// @note Handle room events to get notified once the connection is established or fails.
/// @return LIVEKIT_ERR_NONE, otherwise an error code.
livekit_err_t livekit_room_connect(livekit_room_handle_t handle, const char *server_url, const char *token);

/// @brief Disconnects from a room asynchronously.
/// @param handle[in] Room handle.
/// @note Handle room events to get notified once the disconnection is complete.
/// @return LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_close(livekit_room_handle_t handle);

/// @brief Publishes a data packet to participants in a room asynchronously.
/// @param handle[in] Room handle.
/// @param payload[in] Data to publish and its size.
/// @param options[in] Options for sending the data packet (e.g. reliability, topic, etc.).
/// @return LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_publish_data(livekit_room_handle_t handle, livekit_payload_t *payload, livekit_publish_data_options_t *options);

/// @brief Registers a handler for an RPC method.
livekit_err_t livekit_room_rpc_register(livekit_room_handle_t handle, const char* method, livekit_rpc_handler_t handler);

/// @brief Unregisters a handler for an RPC method.
livekit_err_t livekit_room_rpc_unregister(livekit_room_handle_t handle, const char* method);

#ifdef __cplusplus
}
#endif