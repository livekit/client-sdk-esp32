
#pragma once

#include "media_lib_os.h"
#include "esp_capture.h"
#include "av_render.h"

#include "livekit_rpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Error type returned by all public functions.
typedef enum {
    LIVEKIT_ERR_NONE          =  0,  ///< No error
    LIVEKIT_ERR_INVALID_ARG   = -1,  ///< Invalid argument
    LIVEKIT_ERR_NO_MEM        = -2,  ///< Dynamic memory allocation failed
    LIVEKIT_ERR_ENGINE        = -3,  ///< Engine
    LIVEKIT_ERR_OTHER         = -4,  ///< Other error
    LIVEKIT_ERR_INVALID_STATE = -5,  ///< Invalid state
    LIVEKIT_ERR_SYSTEM_INIT   = -6   ///< System not initialized
} livekit_err_t;

/// Video codec to use within a room.
typedef enum {
    LIVEKIT_VIDEO_CODEC_NONE = 0, ///< No video codec set
    LIVEKIT_VIDEO_CODEC_H264 = 1  ///< H.264 (AVC)
} livekit_video_codec_t;

/// Audio codec to use within a room.
typedef enum {
    LIVEKIT_AUDIO_CODEC_NONE  = 0, ///< No audio codec set
    LIVEKIT_AUDIO_CODEC_G711A = 1, ///< G.711 A-law (PCMA)
    LIVEKIT_AUDIO_CODEC_G711U = 2, ///< G.711 u-law (PCMU)
    LIVEKIT_AUDIO_CODEC_OPUS  = 3  ///< Opus
} livekit_audio_codec_t;

/// Media mode for the room.
typedef enum {
    LIVEKIT_MEDIA_TYPE_NONE = 0,         ///< No media
    LIVEKIT_MEDIA_TYPE_AUDIO = (1 << 0), ///< Audio only
    LIVEKIT_MEDIA_TYPE_VIDEO = (1 << 1), ///< Video only
    LIVEKIT_MEDIA_TYPE_BOTH  = LIVEKIT_MEDIA_TYPE_AUDIO | LIVEKIT_MEDIA_TYPE_VIDEO, ///< Audio and video
} livekit_media_kind_t;

/// Options for the video encoder.
typedef struct {
    livekit_video_codec_t codec;  ///< Codec to use for encoding
    int width;                    ///< Output frame width in pixels
    int height;                   ///< Output frame height in pixels
    int fps;                      ///< Output frame per second
} livekit_video_encode_options_t;

/// Options for the audio encoder.
typedef struct {
    livekit_audio_codec_t codec;  ///< Codec to use for encoding
    uint32_t sample_rate;         ///< Output sample rate in Hz
    uint8_t channel_count;        ///< Output number of channels
} livekit_audio_encode_options_t;

/// @brief Options for publishing media.
typedef struct {
    /// Kind of media that can be published.
    livekit_media_kind_t kind;

    /// Video encoder options.
    /// @note Only required if the room publishes video.
    livekit_video_encode_options_t video_encode;

    /// Audio encoder options.
    /// @note Only required if the room publishes audio.
    livekit_audio_encode_options_t audio_encode;

    /// Capturer to use for obtaining media to publish.
    /// @note Only required if the room publishes media.
    esp_capture_handle_t capturer;
} livekit_pub_options_t;

/// @brief Options for subscribing to media.
typedef struct {
    /// Kind of media that can be subscribed to.
    livekit_media_kind_t kind;

    /// Renderer to use for subscribed media tracks.
    /// @note Only required if the room subscribes to media.
    av_render_handle_t renderer;
} livekit_sub_options_t;

/// Payload containing a pointer to data and its size.
/// @ingroup DataPackets
typedef struct {
    uint8_t *bytes;  ///< Pointer to data
    size_t size;     ///< Size of the data
} livekit_data_payload_t;

/// Information about a data packet received from a remote participant
/// passed to @ref livekit_room_options_t::on_data_received.
/// @ingroup DataPackets
typedef struct {
    /// Received data.
    livekit_data_payload_t payload;

    /// Topic the data was sent under if specified by the sender.
    char* topic;

    /// Identity of the participant who sent the data.
    char* sender_identity;
} livekit_data_received_t;

/// Options for a room passed to @ref livekit_room_create.
/// @ingroup Lifecycle
typedef struct {
    /// Options for publishing media.
    /// @note Only required if the room publishes media.
    livekit_pub_options_t publish;

    /// Options for subscribing to media.
    /// @note Only required if the room subscribes to media.
    livekit_sub_options_t subscribe;

    /// Handler for when an RPC method invoked with @ref livekit_room_rpc_invoke returns a result.
    /// @see RPC
    void (*on_rpc_result)(const livekit_rpc_result_t* result, void* ctx);

    /// Handler for data packets received from remote participants.
    /// @see DataPackets
    void (*on_data_received)(const livekit_data_received_t* data, void* ctx);

    /// User context passed to all handlers.
    void* ctx;
} livekit_room_options_t;

/// @defgroup System System initialization
/// Perform required one-time system initialization.
/// @{

/// Performs one-time system initialization.
///
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise LIVEKIT_ERR_SYSTEM_INIT.
///
/// Invoke this function early in the application's main function before
/// creating a room. Internally, this will setup the media library's thread scheduler.
///
livekit_err_t livekit_system_init(void);

/// @}

/// @defgroup Lifecycle
/// Create and destroy room objects.
/// @{

/// Handle to a room object.
typedef void *livekit_room_handle_t;

/// Creates a room.
/// @param handle[out] Room handle.
/// @param options[in] Options for the new room.
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_create(livekit_room_handle_t *handle, const livekit_room_options_t *options);

/// Destroys a room.
/// @param handle[in] Room handle.
/// @warning For normal connection closure, disconnect the room first using
///          @ref livekit_room_close before destroying the room.
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_destroy(livekit_room_handle_t handle);

/// @}

/// @defgroup Connection
/// Connect and disconnect from a room.
/// @{

/// Connects to a room asynchronously.
/// @param handle[in] Room handle.
/// @param server_url[in] URL of the LiveKit server beginning with "wss://" or "ws://".
/// @param token[in] Server-generated token for authentication.
/// @note Handle room events to get notified once the connection is established or fails.
/// @return @ref LIVEKIT_ERR_NONE, otherwise an error code.
livekit_err_t livekit_room_connect(livekit_room_handle_t handle, const char *server_url, const char *token);

/// Disconnects from a room asynchronously.
/// @param handle[in] Room handle.
/// @note Handle room events to get notified once the disconnection is complete.
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
livekit_err_t livekit_room_close(livekit_room_handle_t handle);

/// @}

/// @defgroup DataPackets Data packets
///
/// Low-level API for high-frequency data exchange.
///
/// - **Sending**: use the @ref livekit_room_publish_data function.
/// - **Receiving**: define a handler function and set it for
///                  @ref livekit_room_options_t::on_data_received in the room options.
///
/// For more information about this feature, see the
/// [LiveKit documentation](https://docs.livekit.io/home/client/data/packets/).
/// @{

/// Options passed to @ref livekit_room_publish_data.
typedef struct {
    /// Data to publish and its size.
    livekit_data_payload_t *payload;

    /// Topic to send the data packet under.
    char* topic;

    /// Whether the data packet is sent using the lossy channel.
    bool lossy;

    /// Identifies of participants to send the data packet to. If not
    /// specified, the data packet is sent to all participants.
    char** destination_identities;

    /// Number of destination identities.
    int destination_identities_count;
} livekit_data_publish_options_t;

/// Publishes a data packet to participants in a room asynchronously.
///
/// @param handle[in] Room handle.
/// @param options[in] Data to send with options (e.g. reliability, topic, etc.).
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
///
/// Example usage:
/// @code
/// const char* command = "G5 I0 J3 P0 Q-3 X2 Y3";
///
/// livekit_payload_t payload = {
///     .bytes = (uint8_t*)command,
///     .size = strlen(command)
/// };
/// livekit_data_publish_options_t options = {
///     .payload = &payload,
///     .topic = "gcode",
///     .lossy = false,
///     .destination_identities = (char*[]){ "printer-1" },
///     .destination_identities_count = 1
/// };
/// livekit_room_publish_data(room_handle, &options);
/// @endcode
///
livekit_err_t livekit_room_publish_data(livekit_room_handle_t handle, livekit_data_publish_options_t *options);

/// @}

/// @defgroup RPC Remote method calls (RPC)
///
/// Use RPC to execute custom methods on other participants in the room and
/// await a response.
///
/// For more information about this feature, see the
/// [LiveKit documentation](https://docs.livekit.io/home/client/data/rpc/).
/// @{

/// Registers a handler for an RPC method.
///
/// Once registered, the method can be invoked by remote participants in the room.
///
/// @param handle[in] Room handle.
/// @param method[in] Name of the method to register.
/// @param handler[in] Handler function to call when the method is invoked by a remote participant.
/// @exception If a handler for the method is already registered, an error is returned.
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
///
livekit_err_t livekit_room_rpc_register(livekit_room_handle_t handle, const char* method, livekit_rpc_handler_t handler);

/// Unregisters a handler for an RPC method.
///
/// @param handle[in] Room handle.
/// @param method[in] Name of the method to unregister.
/// @return @ref LIVEKIT_ERR_NONE if successful, otherwise an error code.
///
livekit_err_t livekit_room_rpc_unregister(livekit_room_handle_t handle, const char* method);

/// @}

#ifdef __cplusplus
}
#endif