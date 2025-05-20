idf_component_register(SRCS "main.c" "board.c" "media_sys.c" "livekit.c" "livekit_core.c" "livekit_rtc.c" "livekit_signaling.c"
                       "protocol/livekit_models.pb.c" "protocol/livekit_metrics.pb.c" "protocol/livekit_rtc.pb.c" "protocol/timestamp.pb.c"
                       INCLUDE_DIRS "." "protocol")
