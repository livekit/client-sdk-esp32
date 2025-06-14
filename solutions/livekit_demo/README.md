# LiveKit Demo

This demo showcases how to use [LiveKit](https://livekit.io) on ESP32-series chips, powered by Espressif's hardware-optimized WebRTC and media components. It demonstrates using LiveKit APIs to join a room and exchange real-time data and media.

## Structure

Application code under [*main/*](./main/) configures the media system and uses the LiveKit APIs to join a room (see [*livekit.h*](./components/livekit/include/livekit.h)). The API is in early development and may undergo breaking changes.

The demo is currently configured to use the [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) board, which features AEC to enable echo-free bidirectional audio. To configure the demo for a different board, please refer to the [*codec_board* README](../../components/codec_board/README.md).

## Sandbox Token Server

In production, you are responsible for generating JWT-based access tokens to authenticate users. However, to simplify setup, this demo is configured to use sandbox tokens. Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project and take note of its ID for the next step.

## Build

To build and run the demo, you will need [IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) release v5.4 or later installed on your system. Configure required settings and build as follows:

1. Set your Wi-Fi SSID and password in [*settings.h*](main/settings.h).
2. Export your Sandbox Token Server's ID (see above):
```sh
export LK_SANDBOX_ID="your-sandbox-id"
```
3. Build, flash, and monitor:
```
idf.py -p YOUR_SERIAL_DEVICE flash monitor
```

### Device path

To determine the path for your board:

- macOS: Run `ls /dev/cu.*` and look for */dev/cu.usbserial-** or similar.
- Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`.
- Windows: Check Device Manager under "Ports (COM & LPT)" for the COM port (e.g. *COM3*).
