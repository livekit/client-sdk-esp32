# Single Codec

Example of connecting to a LiveKit room with bidirectional audio on an ESP32-S3 board that has **one audio codec and no hardware reference channel**.

The `custom_hardware` example assumes a board with a separate capture ADC (ES7210) feeding a 4-channel TDM stream, where one channel carries a speaker reference for on-device echo cancellation. Many low-cost ESP32-S3 audio boards and production modules do not have that second chip: a single ES8311 handles both playback and capture over a 2-channel I2S bus. Running the AEC-based capture path on such a board starves the AEC front end, overflows its ring buffer, and tears the connection down after roughly half a minute — with no obvious error pointing at the cause.

This example takes capture straight from the codec and leaves echo cancellation to the application side, which is where it belongs when the hardware cannot provide a reference signal.

The [GMIC HA-TOYMD](https://github.com/zfygmic/gmic-livekit-esp32) is used as a concrete example, but the approach applies to any single-codec ESP32-S3 board — update the pin definitions and codec address in `main/board.c`.

## Hardware

ESP32-S3 · 4 MB flash · 2 MB quad PSRAM @ 40 MHz · ES8311 codec · Wi-Fi 2.4 GHz only.

### Pin map

| Signal | GPIO |
|---|---|
| I2C SDA | 17 |
| I2C SCL | 18 |
| I2S MCLK | 16 |
| I2S BCLK | 9 |
| I2S WS | 45 |
| I2S DOUT (ESP32 → codec, playback) | 8 |
| I2S DIN (codec → ESP32, microphone) | 10 |
| Speaker amplifier enable | 48 |

Codec I2C address is 7-bit `0x18`. Audio is 16-bit Philips I2S at 16 kHz with a 256× master clock.

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

```ini
CONFIG_LK_EXAMPLE_USE_WIFI=y
CONFIG_LK_EXAMPLE_WIFI_SSID="<your SSID>"
CONFIG_LK_EXAMPLE_WIFI_PASSWORD="<your password>"
```

> **Note:** The ESP32-S3 only supports 2.4 GHz WiFi. WPA3-only networks fail in a way that reads as "network not found".

### Board adaptation

To adapt this example to a different single-codec board:

1. Update the pin definitions at the top of `main/board.c` (I2C, I2S, PA enable).
2. Update the codec I2C address if your board uses different address strapping.
3. Adjust flash size and PSRAM mode in `sdkconfig.defaults` to match your module.

## Build & Flash

Navigate to this directory in your terminal. Run the following command to build your application, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (3200) livekit_example: Room state changed: CONNECTED
```

## Notes

- **Echo cancellation is not performed on-device** in this configuration. In a speakerphone arrangement, echo control has to happen application-side.
- **CPU headroom is limited.** With Opus encode and decode both running, the idle task gets starved; the task watchdog timeout is raised in `sdkconfig.defaults` accordingly. There is little margin left for additional on-device processing.
- **If the device joins the room and publishes a track but no media ever flows**, check whether the server handed it an IPv6 candidate — the ESP32 will not use one. Pin the server to an IPv4 address (`rtc.ips.includes`) before looking anywhere else.

## Next Steps

With a room connection established, you can connect another client (another ESP32, [LiveKit Meet](https://meet.livekit.io), etc.) or dispatch an [agent](https://docs.livekit.io/agents/) to talk with.
