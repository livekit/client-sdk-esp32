# Examples

This directory contains example applications using the LiveKit ESP-32 SDK, demonstrating various features and
use cases. For setup instructions specific to each example, see the example's README.

## Configuration

This guide lists the settings available for all examples. From within the example's root directory, you can set these options in two ways:

1. **Using *menuconfig***: run `idf.py menuconfig`, navigate to the *LiveKit Example* submenu, and configure the
list of available settings.

2. **In an *sdkconfig* file**: place setting key-value pairs in an *sdkconfig* file in the format shown below.

### Codec board type

For examples that support multiple development boards, you can set the model of the board you are using as follows:

```ini
CONFIG_CODEC_BOARD_TYPE="S3_Korvo_V2"
```

See the README for each example for information about supported boards.

### Network

All examples require a network connection in order to connect to a LiveKit server. Choose one of the following connection options:

#### Wi-Fi

Connect using Wi-Fi by specifying your network SSID and password as follows:

```ini
CONFIG_NETWORK_MODE_WIFI=y
CONFIG_WIFI_SSID="<your SSID>"
CONFIG_WIFI_PASSWORD="<your password>"
```

#### Ethernet

You can choose to connect using Ethernet on supported boards such as the [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/user_guide.html).

TODO:

### Room connection

In production, your backend server is responsible for [generating tokens](https://docs.livekit.io/home/server/generating-tokens/) for users to connect to a room.

However, for the purposes of demonstration, each example supports two methods to supply the required tokens without
creating a custom backend. Choose one of the following methods:

#### Option A: Sandbox token server (recommended)

A Sandbox Token Server is simple hosted component that exposes an HTTP endpoint to generate tokens—much like your own
server would in production. To use this option, create a [Sandbox Token Server](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server) for your LiveKit Cloud project, and set its ID in _sdkconfig_:

```ini
CONFIG_LK_USE_SANDBOX=y
CONFIG_LK_SANDBOX_ID="<your sandbox id>"
```

(Optional) If you would like the token to be generated with a specific room or participant name, you can also add the following keys:

```ini
CONFIG_LK_SANDBOX_ROOM_NAME="robot-control"
CONFIG_LK_SANDBOX_PARTICIPANT_NAME="esp-32"
```

#### Option B: Pre-generated token

Connect to a room by proving a manually generated token and custom LiveKit server URL:

```ini
CONFIG_LK_USE_PREGENERATED=y
CONFIG_LK_TOKEN="your-jwt-token"
CONFIG_LK_SERVER_URL="ws://localhost:7880"
```

This option can be useful for local development or troubleshooting. Tokens can be generated using the *token* subcommand on the [LiveKit CLI](https://docs.livekit.io/home/cli/cli-setup/#generate-access-token), and you can follow [this guide](https://docs.livekit.io/home/self-hosting/local/) to install LiveKit server to run from your local machine.

## Troubleshooting

### No serial ports found

If your board's serial port cannot be detected, check to see if it is recognized by your operating system:

- macOS: Run `ls /dev/cu.*` and look for `/dev/cu.usbserial-*` or similar.
- Linux: Run `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`.
- Windows: Check Device Manager under "Ports (COM & LPT)" for the COM port (e.g. _COM3_).

If you see your board listed, you can manually specify its port to *idf.py* as follows:

```
idf.py -P /dev/cu.usbserial-xyz flash monitor
```