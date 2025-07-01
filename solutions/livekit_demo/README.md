# LiveKit Demo

This demo showcases how to use [LiveKit](https://livekit.io) on ESP32-series chips, powered by Espressif's hardware-optimized WebRTC and media components. It demonstrates using LiveKit APIs to join a room and exchange real-time data and media.

## Structure

Application code under [*main/*](./main/) configures the media system and uses the LiveKit APIs to join a room (see [*livekit.h*](./components/livekit/include/livekit.h)). The API is in early development and may undergo breaking changes.

The demo is currently configured to use the [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) board, which features AEC to enable echo-free bidirectional audio. To configure the demo for a different board, please refer to the [*codec_board* README](../../components/codec_board/README.md).

## Sandbox Token Server

In production, you are responsible for generating JWT-based access tokens to authenticate users. However, to simplify setup, this demo is configured to use sandbox tokens. Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project and take note of its ID for the next step.

## Build

To build and run the demo, you will need [IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) release v5.4 or later installed on your system. Configure required settings and build as follows:

### 1. Set network credentials

Set your Wi-Fi SSID and password in your environment:
```sh
export WIFI_SSID='your_wifi_network_name'
export WIFI_PASSWORD='your_wifi_password'
```

### 2. Choose connection method

In production, your backend server is responsible for [generating tokens](https://docs.livekit.io/home/server/generating-tokens/) for users to connect to a room. For demonstration purposes, choose one of the following options to get the demo up and running quickly:

#### Option A: Sandbox token server (recommended)

Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project, and export its ID:

```sh
export LK_SANDBOX_ID="your-sandbox-id"
```

(Optional) If you would like the token to be generated with a specific room or participant name, you can do so as follows:

```sh
export LK_SANDBOX_ROOM_NAME="Meeting"
export LK_SANDBOX_PARTICIPANT_NAME="ESP-32"
```

#### Option B: Pre-generated token

Set your LiveKit server URL and pre-generated token in your environment:

```sh
export LK_TOKEN="your-jwt-token"
export LK_SERVER_URL="wss://your-livekit-server.com"
```

### 3. Set target

```sh
idf.py set-target esp32s3
```

### 4. Build, flash, and monitor:

```sh
idf.py -p YOUR_DEVICE_PATH flash monitor
```

#### Device path

To determine the path for your board:

- macOS: Run `ls /dev/cu.*` and look for */dev/cu.usbserial-** or similar.
- Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`.
- Windows: Check Device Manager under "Ports (COM & LPT)" for the COM port (e.g. *COM3*).