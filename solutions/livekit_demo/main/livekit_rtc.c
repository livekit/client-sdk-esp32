#include "livekit_rtc.h"

int livekit_user_packet_handler(livekit_room_state_t *room, livekit_user_packet_t *user_packet)
{
    // TODO: Handle user packet
    ESP_LOGI(LK_TAG, "Received user packet");
    return 0;
}

int livekit_rpc_request_handler(livekit_room_state_t *room, livekit_rpc_request_t *rpc_request)
{
    // TODO: Handle RPC request packet
    ESP_LOGI(LK_TAG, "Received RPC request");
    return 0;
}

int livekit_rpc_ack_handler(livekit_room_state_t *room, livekit_rpc_ack_t *rpc_ack)
{
    // TODO: Handle RPC ack packet
    ESP_LOGI(LK_TAG, "Received RPC ack");
    return 0;
}

int livekit_rpc_response_handler(livekit_room_state_t *room, livekit_rpc_response_t *rpc_response)
{
    // TODO: Handle RPC response packet
    ESP_LOGI(LK_TAG, "Received RPC response");
    return 0;
}

int livekit_stream_header_handler(livekit_room_state_t *room, livekit_data_stream_header_t *stream_header)
{
    // TODO: Handle stream header packet
    ESP_LOGI(LK_TAG, "Received stream header");
    return 0;
}

int livekit_stream_chunk_handler(livekit_room_state_t *room, livekit_data_stream_chunk_t *stream_chunk)
{
    // TODO: Handle stream chunk packet
    ESP_LOGI(LK_TAG, "Received stream chunk");
    return 0;
}

int livekit_stream_trailer_handler(livekit_room_state_t *room, livekit_data_stream_trailer_t *stream_trailer)
{
    // TODO: Handle stream trailer packet
    ESP_LOGI(LK_TAG, "Received stream trailer");
    return 0;
}

int livekit_rtc_data_channel_handler(livekit_room_state_t *room, uint8_t *data, int size)
{
    livekit_data_packet_t data_packet = {};
    pb_istream_t stream = pb_istream_from_buffer(data, size);

    if (!pb_decode(&stream, LIVEKIT_DATA_PACKET_FIELDS, &data_packet)) {
        ESP_LOGE(LK_TAG, "Failed to decode data packet: %s", stream.errmsg);
        // TODO: A different error code for this?
        return LIVEKIT_ERR_INVALID_ARG;
    }
    switch (data_packet.which_value) {
        case LIVEKIT_DATA_PACKET_USER_TAG:
            return livekit_user_packet_handler(room, &data_packet.value.user);
        case LIVEKIT_DATA_PACKET_RPC_REQUEST_TAG:
            return livekit_rpc_request_handler(room, &data_packet.value.rpc_request);
        case LIVEKIT_DATA_PACKET_RPC_ACK_TAG:
            return livekit_rpc_ack_handler(room, &data_packet.value.rpc_ack);
        case LIVEKIT_DATA_PACKET_RPC_RESPONSE_TAG:
            return livekit_rpc_response_handler(room, &data_packet.value.rpc_response);
        case LIVEKIT_DATA_PACKET_STREAM_HEADER_TAG:
            return livekit_stream_header_handler(room, &data_packet.value.stream_header);
        case LIVEKIT_DATA_PACKET_STREAM_CHUNK_TAG:
            return livekit_stream_chunk_handler(room, &data_packet.value.stream_chunk);
        case LIVEKIT_DATA_PACKET_STREAM_TRAILER_TAG:
            return livekit_stream_trailer_handler(room, &data_packet.value.stream_trailer);
        default:
            ESP_LOGW(LK_TAG, "Received unsupported data packet type: %d", data_packet.which_value);
            return -1;
    }
}

int livekit_rtc_sig_res_handler(livekit_room_state_t *room, uint8_t *data, int size)
{
    ESP_LOGI(LK_TAG, "Received signaling data");

    livekit_signal_response_t* res = (livekit_signal_response_t*)data;
    // TODO: Handle signaling data
    pb_release(LIVEKIT_SIGNAL_RESPONSE_FIELDS, res);
    return 0;
}

int livekit_rtc_data_handler(esp_webrtc_custom_data_via_t via, uint8_t *data, int size, void *ctx)
{
    if (size == 0 || ctx == NULL) return 0;
    livekit_room_state_t *room = (livekit_room_state_t *)ctx;

    switch (via) {
        case ESP_WEBRTC_CUSTOM_DATA_VIA_DATA_CHANNEL:
            return livekit_rtc_data_channel_handler(room, data, size);
        case ESP_WEBRTC_CUSTOM_DATA_VIA_SIGNALING:
            return livekit_rtc_sig_res_handler(room, data, size);
        default: break;
    }
    return 0;
}

int livekit_rtc_event_handler(esp_webrtc_event_t *event, void *ctx)
{
    livekit_room_state_t *room = (livekit_room_state_t *)ctx;

    switch (event->type) {
        case ESP_WEBRTC_EVENT_CONNECTED:
            // TODO: Handle connected
            ESP_LOGI(LK_TAG, "Connected to room");
            break;
        case ESP_WEBRTC_EVENT_CONNECT_FAILED:
            // TODO: Handle connect failed
            ESP_LOGE(LK_TAG, "Failed to connect to room");
            break;
        case ESP_WEBRTC_EVENT_DISCONNECTED:
            // TODO: Handle disconnected
            ESP_LOGI(LK_TAG, "Disconnected from room");
            break;
        case ESP_WEBRTC_EVENT_DATA_CHANNEL_CONNECTED:
            // TODO: Handle data channel connected
            ESP_LOGI(LK_TAG, "Data channel connected");
            break;
        case ESP_WEBRTC_EVENT_DATA_CHANNEL_DISCONNECTED:
            // TODO: Handle data channel disconnected
            ESP_LOGI(LK_TAG, "Data channel disconnected");
            break;
        default: break;
    }

    // TODO check for null
    // TODO: Handle event
    return 0;
}