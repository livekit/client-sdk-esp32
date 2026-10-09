# LiveKit ESP32-S31 board support

This component provides the audio codec, I2S, amplifier, camera, and RGB
status-LED initialization used by LiveKit's ESP32-S31 examples. It currently
supports:

- `ESP32_S31_KORVO_1`
- `ESP32_S31_FUNCTION_COREBOARD_1`

Initialize one profile with `livekit_s31_board_init()`, then pass the returned
playback and record handles to `esp_capture` and `av_render`. Keep both
hardware-facing directions at 48 kHz, 16-bit stereo and use
`livekit_s31_audio_render_alloc()` for buffered playback. The media pipelines
convert the requested Opus format to/from this fixed I2S format. See
[`docs/esp32s31.md`](../../docs/esp32s31.md) for wiring, ESP-IDF requirements,
configuration, and tested limitations.

Before starting the singleton capture source, call
`livekit_s31_board_configure_capture(source, use_aec)`. This fixes the native
bus format and, without AEC, duplicates Korvo's left microphone slot before
mono conversion. Keep AEC reference slots separate. Microphone gain is applied
by each board adapter; `CONFIG_LK_S31_KORVO_MIC_GAIN_DB` and
`CONFIG_LK_S31_FUNCTION_CORE_MIC_GAIN_DB` both default to 30 dB.

Function-CoreBoard gives the ES8311 driver separate ADC and DAC interfaces
sharing the same buses. The driver can then keep microphone clocks running
when playback closes; only the DAC interface controls the speaker amplifier.

`livekit_s31_board_camera_init()` registers the Korvo-1 onboard OV3660 DVP
camera as `ESP_VIDEO_DVP_DEVICE_NAME` (`/dev/video2`). Function-CoreBoard-1 returns
`ESP_ERR_NOT_SUPPORTED` because it has no onboard camera.

The component intentionally covers only hardware used by the existing LiveKit
room, audio, video-publish, and LED examples. Display, Ethernet, USB host, and
additional peripheral setup are outside its initial scope.

The S31 examples expose `CONFIG_LK_EXAMPLE_ENABLE_AEC`. With AEC enabled,
`livekit_s31_aec_source_new()` creates a capture source using the codec's
microphone and digital DAC-reference slots. It keeps the shared bus at
48 kHz and produces 16 kHz mono PCM using VOIP_HIGH_PERF echo cancellation.
Use the board-specific slot mapping and preserve separate reference samples;
the normal mono-conversion source must not precede this source. See
[`livekit_s31_aec.h`](include/livekit_s31_aec.h) for ownership and lifecycle.

The renderer uses a 64 KiB ring and a dedicated codec writer. It primes with
200 ms of PCM and re-primes when its queue empties. A 200 ms idle deadline
and end-of-stream handling release short clips; arriving data extends that
deadline. Close cancels queued PCM and waits for the in-progress codec write;
there is no fixed-delay task deletion. Software latency reporting excludes
the codec/DMA delay. The render helper does not initialize a board, so the
custom-hardware example can reuse it with its own codec handles.
