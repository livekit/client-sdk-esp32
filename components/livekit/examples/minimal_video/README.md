# Minimal Video

Basic example of connecting to a LiveKit room with bidirectional audio and video publishing.

## Configuration

> [!TIP]
> Options can either be set through *menuconfig* or added to *sdkconfig* as shown below.

### Credentials

**Option A**: Enable the Development token server under **Project settings >
Sandbox** in [LiveKit Cloud](https://cloud.livekit.io/projects/p_/sandbox),
then use its Token server ID. The configuration field retains the `SANDBOX`
name for API compatibility:

```ini
CONFIG_LK_EXAMPLE_USE_SANDBOX=y
CONFIG_LK_EXAMPLE_SANDBOX_ID="my-project-xxxxxx"
```

**Option B**: Specify a server URL and pregenerated token:

```ini
CONFIG_LK_EXAMPLE_USE_PREGENERATED=y
CONFIG_LK_EXAMPLE_TOKEN="your-jwt-token"
CONFIG_LK_EXAMPLE_SERVER_URL="ws://localhost:7880"
```

### Network

Connect using WiFi as follows:

```ini
CONFIG_LK_EXAMPLE_USE_WIFI=y
CONFIG_LK_EXAMPLE_WIFI_SSID="<your SSID>"
CONFIG_LK_EXAMPLE_WIFI_PASSWORD="<your password>"
```

Or using Ethernet (ESP32-P4 only):

```ini
CONFIG_LK_EXAMPLE_USE_ETHERNET=y
```

### Development Board

On ESP32-S3 and ESP32-P4, this example uses the Espressif [*codec_board*](https://components.espressif.com/components/tempotian/codec_board/) component to access board-specific peripherals for media capture and rendering. Supported boards are [defined here](https://github.com/espressif/esp-webrtc-solution/blob/65d13427dd83c37264b6cff966d60af0f84f649c/components/codec_board/board_cfg.txt). Locate the name of your board, and set it as follows:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_P4_DEV_V14"
```

ESP32-S31-Korvo-1 is supported through the repository's `livekit/s31_board`
component and its onboard OV3660 DVP camera:

```sh
idf.py --preview set-target esp32s31
idf.py --preview build
idf.py --preview -p PORT -b 921600 flash monitor
```

S31 capture and playback share a fixed 48 kHz stereo I2S bus. On Korvo-1,
capture uses 30 dB microphone gain and duplicates the left microphone slot
before mono conversion, matching the audio-only examples and avoiding signal
cancellation from averaging the raw slots. This is not acoustic echo cancellation.

The initial S31 profile publishes 240 x 240 H.264 at 10 fps. It uses the same
RGB565-BE 240 x 240 at 24 fps OV3660 mode as Espressif's Korvo-1 factory demo,
drops the capture rate to 10 fps before color conversion, and uses the
S31-compatible software H.264 encoder. The lower profile reflects the lack of
the P4's dedicated H.264 encoder. Hardware testing confirmed camera detection,
correct exposure and orientation, 24-to-10 fps conversion, H.264 publication,
and a three-minute stream without a task-watchdog warning. A subsequent
audio-plus-camera retest with the frame-boundary scheduling correction ran
for 10.5 minutes: the subscriber received 5,661 frames (about 9 fps average)
and continuous audio, including three playback-track replacements, without
task-watchdog warnings. A heap/PSRAM trend was not instrumented in that run.
The S31 capture wrapper blocks for one scheduler tick at each camera frame
boundary so a continuously busy software encoder cannot starve the idle task.
The 10 fps setting is a target; received frame rate also depends on scene
complexity and CPU load. Keep the task watchdog enabled during validation.

The v0.3.11 refresh, with AEC enabled, passed the same sustained receiver
test: 6,294 frames at 240 x 240 over 628.216 seconds, averaging 10.017 fps,
with 62,999 audio frames and a maximum video gap of 362 milliseconds.
No firmware or watchdog errors were logged. This run checked continuous
reception; visual/listening acceptance and overlapping-speech returns were
not repeated. See the [refresh validation record](../../../../docs/s31-refresh-validation.md).

ESP32-S31-Function-CoreBoard-1 has no onboard camera, so this example requires
an external camera plus a board-specific adapter on that board. Such an adapter
must provide a camera supported by `esp_cam_sensor`/`esp_video`, its SCCB/I2C
bus, XCLK, PCLK, VSYNC, HSYNC, eight DVP data signals, and any required power,
reset, or power-down controls. This repository does not claim a reference pin
mapping for that add-on. Its audio-only LiveKit support is covered by the
`minimal` example.

## Build & Flash

Navigate to this directory in your terminal. Run the following command to build your application, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (19508) livekit_example: Room state changed: Connected
```

On Korvo-1, camera initialization should register the esp-video DVP device as
`/dev/video2`, and the room should reach `Connected`. Initial hardware testing
confirmed that LiveKit Cloud receives an Opus microphone track and a 240 x 240
H.264 camera track. For final validation, use a second LiveKit participant to
inspect the image and receive the video continuously for at least ten minutes.
Record the observed frame rate, heap/PSRAM trend, and any dropped-frame or
encoder errors.

When the board sits next to the receiving computer, use headphones or mute
the computer's playback while sending its microphone to the board. A nearby
speaker can feed back into the board microphone even if the computer's own
microphone is disabled. On S31, AEC is enabled by default through
`CONFIG_LK_EXAMPLE_ENABLE_AEC=y`. Physical-button mute control is not provided.

## Next Steps

With a room connection established, you can connect another client (another ESP32, [LiveKit Meet](https://meet.livekit.io), etc.) or dispatch an [agent](https://docs.livekit.io/agents/) to talk with.
