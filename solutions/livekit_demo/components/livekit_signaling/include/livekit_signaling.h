
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void *livekit_sig_handle_t;

typedef enum {
    LIVEKIT_SIG_ERR_NONE        =  0,
    LIVEKIT_SIG_ERR_INVALID_ARG = -1,
    LIVEKIT_SIG_ERR_NO_MEM      = -2,
    LIVEKIT_SIG_ERR_WEBSOCKET   = -3,
    LIVEKIT_SIG_ERR_INVALID_URL = -4,
    LIVEKIT_SIG_ERR_MESSAGE     = -5,
    LIVEKIT_SIG_ERR_OTHER       = -6,
    // TODO: Add more error cases as needed
} livekit_sig_err_t;

typedef struct {
    void* ctx;
    void (*on_connect)(void *ctx);
    void (*on_disconnect)(void *ctx);
    void (*on_error)(void *ctx);
    // TODO: Consider adding additional callbacks for specific message types
    void (*on_message)(livekit_signal_response_t *message, void *ctx);
} livekit_sig_options_t;

livekit_sig_err_t livekit_sig_create(livekit_sig_options_t *options, livekit_sig_handle_t *handle);
livekit_sig_err_t livekit_sig_destroy(livekit_sig_handle_t handle);

/// @brief Establishes the WebSocket connection
/// @note This function will close the existing connection if already connected.
livekit_sig_err_t livekit_sig_connect(const char* server_url, const char* token, livekit_sig_handle_t handle);

/// @brief Closes the WebSocket connection
/// @param force If true, the connection will be closed immediately without sending a leave message.
livekit_sig_err_t livekit_sig_close(bool force, livekit_sig_handle_t handle);

livekit_sig_err_t livekit_sig_send_message(livekit_signal_request_t *request, livekit_sig_handle_t handle);

#ifdef __cplusplus
}
#endif