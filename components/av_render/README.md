# AV_Render

`AV_Render` is a simple player designed for video and audio playback. It primarily supports "push" playback model, where the user first provides stream information and then pushes the stream data to `av_render`. The player checks the input stream's codec and uses appropriate audio and video decoders for processing. The decoded output frame data is sent to the corresponding renderer for final output (e.g., audio playback via codec, video playback on LCD).

## Abstraction

### Render Implementations

Users can select the appropriate renderer based on their specific use case. We also provide default implementations for common scenarios, such as:

- **Audio Rendering:** `av_render_alloc_i2s_render` — outputs audio through I2S.
- **Video Rendering:** `av_render_alloc_lcd_render` — renders video through `esp_lcd`.

## Simple Usage

The overall flow for audio and video playback is as follows:

1. `av_render_open`
2. `av_render_add_audio_stream`
3. `av_render_add_video_stream`
4. `av_render_add_audio_data`
5. `av_render_add_video_data`
6. `av_render_close`

### Resetting the Playback

If you want to clear the current stream and start a new one, call `av_render_reset`.

### Resource usage

Users can configure resource usage more precisely to meet their specific needs. This includes options such as whether an additional thread is required for decoding, as well as how much data can be buffered by the decoder and renderer.

To customize these settings, you can use the `av_render_cfg_t` configuration structure or the following API functions:

- `av_render_config_audio_fifo` — Configure the audio buffer size for the decoder and renderer.
- `av_render_config_video_fifo` — Configure the video buffer size for the decoder and renderer.
