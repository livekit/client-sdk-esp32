# Minimal

Basic example of connecting to a LiveKit room with bidirectional audio.

## Configuration

> [!TIP]
> Options can either be set through *menuconfig* or added to *sdkconfig* as shown below.

### Credentials

**Option A**: Use a LiveKit Sandbox to get up and running quickly. Setup a LiveKit Sandbox from your [Cloud Project](https://cloud.livekit.io/projects/p_/sandbox), and use its ID in your configuration:

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

On established targets this example uses the Espressif [*codec_board*](https://components.espressif.com/components/tempotian/codec_board/) component to access board-specific peripherals for media capture and rendering. Supported boards are [defined here](https://github.com/espressif/esp-webrtc-solution/blob/65d13427dd83c37264b6cff966d60af0f84f649c/components/codec_board/board_cfg.txt). Locate the name of your board, and set it as follows:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_S3_BOX_3"
```

For ESP32-S31, LiveKit provides reusable profiles for both official audio
boards. The target-specific defaults select the Korvo-1:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_S31_KORVO_1"
```

For the Function-CoreBoard-1, set:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_S31_FUNCTION_COREBOARD_1"
```

The Korvo-1 profile uses its ES8389 codec and stereo speaker connectors. The
Function-CoreBoard-1 profile uses its mono ES8311 codec, onboard microphone,
and J9 speaker connector. AEC is off by default on S31 for this example; set
`CONFIG_LK_EXAMPLE_ENABLE_AEC=y` when the board plays remote audio while
capturing, as the `voice_agent` example does.

Without AEC, Korvo capture keeps the shared 48 kHz stereo bus but selects the
left input before mono conversion, avoiding cancellation from averaging the
raw slots. Its microphone gain is set to 30 dB. Speaker settings and codec
reference routing are unchanged.

ESP32-S31 support requires ESP-IDF 6.1. From an ESP-IDF 6.1 shell, configure
the preview target before setting credentials:

```sh
idf.py --preview set-target esp32s31
idf.py --preview menuconfig
```

## Build & Flash

Navigate to this directory in your terminal. Run the following command to build your application, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

For ESP32-S31, include the preview flag and replace `PORT` with the board's
serial port:

```sh
idf.py --preview build
idf.py --preview -p PORT flash monitor
```

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (19508) livekit_example: Room state: Connected
```

## Next Steps

With a room connection established, you can connect another client (another ESP32, [LiveKit Meet](https://meet.livekit.io), etc.) or dispatch an [agent](https://docs.livekit.io/agents/) to talk with.
