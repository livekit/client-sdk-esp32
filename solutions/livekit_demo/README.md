# LiveKit Demo

This demo showcases how to use [LiveKit](https://livekit.io) on ESP32-series chips, powered by Espressif's hardware-optimized WebRTC and media components. It demonstrates using LiveKit APIs to join a room and exchange real-time data and media.

## Structure

Application code under [*main/*](./main/) configures the media system and uses the LiveKit APIs to join a room (see [*livekit.h*](./components/livekit/include/livekit.h)). The API is in early development and may undergo breaking changes.

The demo is currently configured to use the [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) board, which features AEC to enable echo-free bidirectional audio. To configure the demo for a different board, please refer to the [*codec_board* README](../../components/codec_board/README.md).

## Sandbox Token Server

In production, you are responsible for generating JWT-based access tokens to authenticate users. However, to simplify setup, this demo is configured to use sandbox tokens. Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project and take note of its ID for the next step.

## Build

To build and run the demo, you will need [IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) release v5.4 or later installed on your system. Configure required settings and build as follows:

1. Export your Wi-Fi SSID and password:
```sh
export WIFI_SSID='your_wifi_network_name'
export WIFI_PASSWORD='your_wifi_password'
```

2. **Option A: Use Sandbox Token Server (for demo/evaluation)**
   Export your Sandbox Token Server's ID:
   ```sh
   export LK_SANDBOX_ID="your-sandbox-id"
   ```

   **Option B: Use Pre-generated Token (for production)**
   Export your LiveKit token and server URL:
   ```sh
   export LK_TOKEN="your-jwt-token"
   export LK_SERVER_URL="wss://your-livekit-server.com"
   ```

3. Export your LiveKit room & participant configuration:
```sh
export ROOM_NAME='esp32'
export PARTICIPANT_NAME='esp32'
```

4. Set target:
```sh
idf.py set-target esp32s3
```

5. Build, flash, and monitor:
```sh
idf.py -p YOUR_SERIAL_DEVICE flash monitor
```

### Device path

To determine the path for your board:

- macOS: Run `ls /dev/cu.*` and look for */dev/cu.usbserial-** or similar.
- Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`.
- Windows: Check Device Manager under "Ports (COM & LPT)" for the COM port (e.g. *COM3*).

## Appendix: Creating LiveKit Tokens

For production use, you'll need to generate JWT tokens for authentication. The easiest way is using the LiveKit CLI tool.

see [LiveKit CLI Doc](https://docs.livekit.io/home/cli/cli-setup/#generate-access-token)

### Creating a Token

Set your API credentials as environment variables:

```sh
export LIVEKIT_API_KEY="your-api-key"
export LIVEKIT_API_SECRET="your-api-secret"
export LIVEKIT_URL

export ROOM_NAME="esp32"
export PARTICIPANT_NAME="esp32"
```

Generate a token for the ESP32:

```sh
lk token create \
  --api-key $LIVEKIT_API_KEY --api-secret $LIVEKIT_API_SECRET \
  --join --room $ROOM_NAME --identity $PARTICIPANT_NAME \
  --valid-for 24h
```

This command creates a token that:
- Allows joining the room named `test_room`
- Identifies the participant as `esp32_user`
- Is valid for 24 hours
- Can be used with the `LK_TOKEN` environment variable

### Using the Token

Copy the generated token and use it with the demo:

```sh
export LK_TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
export LK_SERVER_URL=$LIVEKIT_URL
```
