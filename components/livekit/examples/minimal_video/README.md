# Minimal Video

Basic example of connecting to a LiveKit room with bidirectional audio and video publishing.

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

This example uses the Espressif [*codec_board*](https://components.espressif.com/components/tempotian/codec_board/) component to access board-specific peripherals for media capture and rendering. Supported boards are [defined here](https://github.com/espressif/esp-webrtc-solution/blob/65d13427dd83c37264b6cff966d60af0f84f649c/components/codec_board/board_cfg.txt). Locate the name of your board, and set it as follows:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_P4_DEV_V14"
```

## Build & Flash

Navigate to this directory in your terminal. Run the following command to build your application, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (19508) livekit_example: Room state: Connected
```

## Next Steps

With a room connection established, you can connect another client (another ESP32, [LiveKit Meet](https://meet.livekit.io), etc.) or dispatch an [agent](https://docs.livekit.io/agents/) to talk with.
