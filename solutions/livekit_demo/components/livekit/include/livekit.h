
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
    char *server_url;
    char *token;
    // TODO: Media provider, event handler, etc.
} livekit_join_options_t;

livekit_err_t livekit_room_create(livekit_room_handle_t *handle);
livekit_err_t livekit_room_destroy(livekit_room_handle_t handle);

livekit_err_t livekit_room_join(livekit_room_handle_t handle, livekit_join_options_t *options);
livekit_err_t livekit_room_leave(livekit_room_handle_t handle);

#ifdef __cplusplus
}
#endif