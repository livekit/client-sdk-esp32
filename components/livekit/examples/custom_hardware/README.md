# Custom Hardware

Example of connecting to a LiveKit room with bidirectional audio on custom ESP32-S3 hardware. Unlike the `minimal` example (which uses the `codec_board` abstraction), this example manually initializes I2C, I2S, and audio codecs (ES8311 + ES7210) using pin assignments extracted from the board schematic. Use this as a starting point when your board isn't supported by `codec_board`.

The [Waveshare ESP32-S3-Touch-LCD-1.83](https://www.waveshare.com/esp32-s3-touch-lcd-1.83.htm) is used as a concrete example, but the approach works for any ESP32-S3 board with I2S audio codecs — just update the pin definitions and codec configuration in `board.c`.

For a step-by-step walkthrough of this example, see the companion blog post: [Building a voice agent frontend on custom ESP32 hardware](https://livekit.io/blog/esp32-custom-hardware-quickstart).

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

> **Note:** The ESP32-S3 only supports 2.4 GHz WiFi.

### Board adaptation

This example is configured for the Waveshare ESP32-S3-Touch-LCD-1.83. To adapt it to your own board:

1. Update the pin definitions at the top of `main/board.c` (I2C, I2S, PA enable).
2. Update the codec I2C addresses if your board uses different address strapping.
3. If your board has no PMU, remove the `init_pmu()` call from `board_init()`.
4. If your board uses different codecs, replace the ES8311/ES7210 initialization with the appropriate driver calls.

## Build & Flash

Navigate to this directory in your terminal. Run the following command to build your application, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (3200) livekit_example: Room state changed: CONNECTED
```

## Next Steps

With a room connection established, you can connect another client (another ESP32, [LiveKit Meet](https://meet.livekit.io), etc.) or dispatch an [agent](https://docs.livekit.io/agents/) to talk with.
