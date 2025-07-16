# LiveKit Demo

This demo showcases how to use [LiveKit](https://livekit.io) on ESP32-series chips, powered by Espressif's hardware-optimized WebRTC and media components. It demonstrates using LiveKit APIs to join a room and exchange real-time data and media.

## Structure

Application code under [*main/*](./main/) configures the media system and uses the LiveKit APIs to join a room (see [*livekit.h*](./components/livekit/include/livekit.h)). The API is in early development and may undergo breaking changes.

The demo is currently configured to use the [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) board, which features AEC to enable echo-free bidirectional audio. To configure the demo for a different board, please refer to the [*codec_board* README](../../components/codec_board/README.md).

## Sandbox Token Server

In production, you are responsible for generating JWT-based access tokens to authenticate users. However, to simplify setup, this demo is configured to use sandbox tokens. Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project and take note of its ID for the next step.

## Build

### 1. Configure required settings

To configure the example from the command line using _menuconfig_, run:
```sh
idf.py menuconfig
```
Then navigate to the _LiveKit Example_ menu.

Alternatively, you can set these options directly in an _sdkconfig_ file located in this directory as shown below:

#### Network connection

Connect using Wi-Fi:

```sh
CONFIG_NETWORK_MODE_WIFI=y
CONFIG_WIFI_SSID="<your SSID>"
CONFIG_WIFI_PASSWORD="<your password>"
```

#### Room connection

In production, your backend server is responsible for [generating tokens](https://docs.livekit.io/home/server/generating-tokens/) for users to connect to a room. For demonstration purposes, choose one of the following options to get the demo up and running quickly:

##### Option A: Sandbox token server (recommended)

Create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project, and set its ID in _sdkconfig_:

```sh
CONFIG_LK_USE_SANDBOX=y
CONFIG_LK_SANDBOX_ID="<your sandbox id>"
```

(Optional) If you would like the token to be generated with a specific room or participant name, you can also add the following keys:

```sh
CONFIG_LK_SANDBOX_ROOM_NAME="robot-control"
CONFIG_LK_SANDBOX_PARTICIPANT_NAME="esp-32"
```

##### Option B: Pre-generated token

Set your LiveKit server URL and pre-generated token in _sdkconfig_:

```sh
CONFIG_LK_USE_PREGENERATED=y
CONFIG_LK_TOKEN="your-jwt-token"
CONFIG_LK_SERVER_URL="wss://your-livekit-server.com"
```

### 3. Set target

```sh
idf.py set-target esp32s3
```

### 4. Build, flash, and monitor:

With your dev board connected, run the following to build, flash, and monitor in a single command:
```sh
idf.py -p YOUR_DEVICE_PATH flash monitor
```

#### Device path

To determine the path for your board:

- macOS: Run `ls /dev/cu.*` and look for */dev/cu.usbserial-** or similar.
- Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`.
- Windows: Check Device Manager under "Ports (COM & LPT)" for the COM port (e.g. *COM3*).