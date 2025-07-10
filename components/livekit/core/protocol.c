#include "protocol.h"

const char* livekit_protocol_sig_res_name(pb_size_t which_message)
{
    switch (which_message) {
        case LIVEKIT_PB_SIGNAL_RESPONSE_JOIN_TAG: return "Join";
        case LIVEKIT_PB_SIGNAL_RESPONSE_ANSWER_TAG: return "Answer";
        case LIVEKIT_PB_SIGNAL_RESPONSE_OFFER_TAG: return "Offer";
        case LIVEKIT_PB_SIGNAL_RESPONSE_TRICKLE_TAG: return "Trickle";
        case LIVEKIT_PB_SIGNAL_RESPONSE_UPDATE_TAG: return "Update";
        case LIVEKIT_PB_SIGNAL_RESPONSE_TRACK_PUBLISHED_TAG: return "TrackPublished";
        case LIVEKIT_PB_SIGNAL_RESPONSE_LEAVE_TAG: return "Leave";
        case LIVEKIT_PB_SIGNAL_RESPONSE_MUTE_TAG: return "Mute";
        case LIVEKIT_PB_SIGNAL_RESPONSE_SPEAKERS_CHANGED_TAG: return "SpeakersChanged";
        case LIVEKIT_PB_SIGNAL_RESPONSE_ROOM_UPDATE_TAG: return "RoomUpdate";
        case LIVEKIT_PB_SIGNAL_RESPONSE_CONNECTION_QUALITY_TAG: return "ConnectionQuality";
        case LIVEKIT_PB_SIGNAL_RESPONSE_STREAM_STATE_UPDATE_TAG: return "StreamStateUpdate";
        case LIVEKIT_PB_SIGNAL_RESPONSE_SUBSCRIBED_QUALITY_UPDATE_TAG: return "SubscribedQualityUpdate";
        case LIVEKIT_PB_SIGNAL_RESPONSE_SUBSCRIPTION_PERMISSION_UPDATE_TAG: return "SubscriptionPermissionUpdate";
        case LIVEKIT_PB_SIGNAL_RESPONSE_REFRESH_TOKEN_TAG: return "RefreshToken";
        case LIVEKIT_PB_SIGNAL_RESPONSE_TRACK_UNPUBLISHED_TAG: return "TrackUnpublished";
        case LIVEKIT_PB_SIGNAL_RESPONSE_PONG_TAG: return "Pong";
        case LIVEKIT_PB_SIGNAL_RESPONSE_RECONNECT_TAG: return "Reconnect";
        case LIVEKIT_PB_SIGNAL_RESPONSE_PONG_RESP_TAG: return "PongResp";
        case LIVEKIT_PB_SIGNAL_RESPONSE_SUBSCRIPTION_RESPONSE_TAG: return "SubscriptionResponse";
        case LIVEKIT_PB_SIGNAL_RESPONSE_REQUEST_RESPONSE_TAG: return "RequestResponse";
        case LIVEKIT_PB_SIGNAL_RESPONSE_TRACK_SUBSCRIBED_TAG: return "TrackSubscribed";
        case LIVEKIT_PB_SIGNAL_RESPONSE_ROOM_MOVED_TAG: return "RoomMoved";
        default: return "Unknown";
    }
}