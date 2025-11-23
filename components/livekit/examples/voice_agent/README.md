# Voice Agent

Example of using LiveKit to enable bidirectional voice chat with an AI agent built with [LiveKit Agents](https://docs.livekit.io/agents/).

The agent in this example can interact with hardware in response to user requests. Below is an example of a conversation between a user and the agent:

> **User:** What is the current CPU temperature? \
> **Agent:** The CPU temperature is currently 33°C.

> **User:** Turn on the blue LED. \
> **Agent:** *[turns blue LED on]*

> **User:** Turn on the yellow LED. \
> **Agent:** I'm sorry, the board does not have a yellow LED.

## Configuration

> [!TIP]
> Options can either be set through *menuconfig* or added to *sdkconfig* as shown below.

### Credentials

> [!IMPORTANT]
> This example comes with a pre-configured Sandbox that automatically dispatches the hosted agent included with this example. Feel free to ignore this section until you are ready to use your own cloud project.

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

By default, this example targets the [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html) development board, using its corresponding [board support package](https://components.espressif.com/components/espressif/esp32_s3_korvo_2/) (BSP) to access the LED peripherals for the agent to control. If you wish to target a different board, this dependency can be easily removed or replaced.

If using a board other than the ESP32-S3-Korvo-2, note that this example uses the Espressif [*codec_board*](https://components.espressif.com/components/tempotian/codec_board/) component to access board-specific peripherals for media capture and rendering. Supported boards are [defined here](https://github.com/espressif/esp-webrtc-solution/blob/65d13427dd83c37264b6cff966d60af0f84f649c/components/codec_board/board_cfg.txt). Locate the name of your board, and set it as follows:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="S3_Korvo_V2"
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

If you are using the provided Sandbox, you should be able to converse with the agent at this point. Start by asking "What's the CPU temperature?"

## Next Steps

Explore how the agent is built (see its source in the *./agent* directory). If you are unfamiliar with [LiveKit Agents](https://docs.livekit.io/agents/), refer to the [Voice AI Quickstart](https://docs.livekit.io/agents/start/voice-ai/) to learn how you can build upon the example agent or create your own from scratch.
