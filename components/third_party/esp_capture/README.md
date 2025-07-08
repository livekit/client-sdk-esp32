# ESP_Capture

`esp_capture` is an integrated capture module for capturing audio and video data. It supports various audio and video capture devices and codecs, allowing users to easily acquire audio/video frame data or container data.

## Features

- **Capture Devices**: Abstracted into audio or video sources. Users can either use existing capture devices or add custom ones via provided interfaces.
- **Audio/Video Codecs**: Support for a wide range of codecs。 Users can use `menuconfig` to remove unused codecs to reduce the final binary size.
- **Muxer Support**: Support Saving muxed container data to storage or send it through a network.
- **Capture Path**: Simple capture paths for single audio and video codec outputs, with multiple path support under testing.

---

## Capture Devices

Capture devices are abstracted into **audio sources** and **video sources**. Users can:

1. Add custom capture devices using the provided interfaces.
2. Use existing supported devices:

### Supported Audio Capture Devices:
- `esp_capture_new_audio_codec_src`: Supports I2S devices using the `esp_codec_dev` handle.
- `esp_capture_new_audio_aec_src`: Supports I2S devices with AEC using the `esp_codec_dev` handle.

### Supported Video Capture Devices:
1. `esp_capture_new_video_v4l2_src`: Supports MIPI CSI cameras and DVP cameras on ESP32-P4.
2. `esp_capture_new_video_dvp_src`: Supports DVP cameras on ESP32-S3 and other ESP platforms.

---

## Capture Codecs

Users can register video and audio codecs via the following APIs:

- `esp_video_enc_register_default`
- `esp_audio_enc_register_default`

### Audio Codecs:
- `G711A`
- `G711U`
- `AAC`
- `OPUS`

### Video Codecs:
- `MJPEG`
- `H.264` (Baseline profile only)

Unused codecs can be deselected via `menuconfig` to reduce the final image size.

---

## Muxer Support

Muxers allow user mux audio and video data into container formats:

- **Supported Formats**:
  - `MP4`: Supports saving to storage only.
  - `TS`: Supports saving to storage and streaming output both.

---

## Capture Path

Each capture path supports one audio and video codec or muxed data output.

- **Simple Capture**: Currently supports one capture path.
- **Multiple Path Support**: Developped based on ESP-GMF (under testing, not released yet).

---

## Simple Usage

Below are the steps to use `esp_capture` for audio and video capture:

1. **Register Codecs**:
   - Register audio codecs using `esp_audio_enc_register_default`.
   - Register video codecs using `esp_video_enc_register_default`.

2. **Select Capture Devices**:
   - Use `esp_capture_new_audio_codec_src` to create an audio source.
   - Use `esp_capture_new_video_v4l2_src` to create a video source.

3. **Build Capture Path**:
   - Use `esp_capture_build_simple_path` to build a capture path.
   - Use the path to create a capture handle with `esp_capture_open`.

4. **Setup and Enable Path**:
   - Configure codec settings using `esp_capture_setup_path`.
   - Enable the capture path using `esp_capture_enable_path`.

5. **Start Capture**:
   - Call `esp_capture_start` to begin capturing.

6. **Acquire and Release Frame Data**:
   - Call `esp_capture_acquire_path_frame` to retrieve audio, video, or muxed data.
   - Call `esp_capture_release_path_frame` to release the frame data when done.

7. **Stop Capture**:
   - Call `esp_capture_stop` to end the capture session.