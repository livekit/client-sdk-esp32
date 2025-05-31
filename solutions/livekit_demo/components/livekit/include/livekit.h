
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void *livekit_room_handle_t;

typedef enum {
    LIVEKIT_ERR_NONE        =  0,
    LIVEKIT_ERR_INVALID_ARG = -1,
    LIVEKIT_ERR_NO_MEM      = -2,
    LIVEKIT_ERR_OTHER       = -3,
    // TODO: Add more error cases as needed
} livekit_err_t;

typedef struct {
    // TODO: Media provider, event handler, etc.
} livekit_room_options_t;

livekit_err_t livekit_room_create(livekit_room_options_t *options, livekit_room_handle_t *handle);
livekit_err_t livekit_room_destroy(livekit_room_handle_t handle);

livekit_err_t livekit_room_connect(const char *server_url, const char *token, livekit_room_handle_t handle);
livekit_err_t livekit_room_close(livekit_room_handle_t handle);
// TODO: Add disconnect reason argument

#ifdef __cplusplus
}
#endif