# Voice Agent

Example application combining this SDK with [LiveKit Agents](https://docs.livekit.io/agents/), enabling bidirectional voice communication with an AI agent from an ESP32. The agent can interact with hardware in response to user requests. Below is an example of a conversation between a user and the agent:

> **User:** What is the current CPU temperature? \
> **Agent:** The CPU temperature is currently 33°C.

> **User:** Turn on the blue LED. \
> **Agent:** *[turns blue LED on]*

> **User:** Turn on the yellow LED. \
> **Agent:** I'm sorry, the board does not have a yellow LED.

## Structure

The example includes both an ESP32 application for connecting to a LiveKit room, and the definition for the agent the user interacts with:
```txt
.
├── agent/
│   └── agent.py (Agent definition)
└── main/ (ESP32 application)
```

## Requirements

- Software:
    - [IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) release v5.4 or later
- Hardware
    - Dev board: [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)
    - Two micro USB cables: one for power, one for flashing
    - Mono enclosed speaker (example from [Adafruit](https://www.adafruit.com/product/3351))

## Quick start

> [!NOTE]
> The example comes pre-configured to connect to the agent hosted in
> LiveKit Cloud. If you would like to run the agent yourself, see the the agent's [README](./agent/README.md).

Begin in your terminal by navigating to this example's root directory: *[examples/voice_agent](./examples/voice_agent/)*.

### 1. Configuration

At minimum, the example requires a network connection. To configure Wi-Fi and other settings, launch *menuconfig* from your terminal:
```sh
idf.py menuconfig
```



With *menuconfig* open, navigate to the *LiveKit Example* menu and configure the following settings:

- Network → Wi-Fi SSID
- Network → Wi-Fi password

For more information about available options, including how to connect to your own LiveKit Cloud project, please refer to [this guide](../README.md#configuration).

### 2. Build & flash

Begin by connecting your dev board via USB. With the board connected, use the following command
to build the example, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running on device, the example will establish a network connection and then connect to a LiveKit room. Once connected, you will see the following log message:

```sh
I (19508) livekit_example: Room state: connected
```

Shortly after the room is connected, the agent participant will join and begin speaking. If you encounter any issues during this process, please refer to the example [troubleshooting guide](../README.md/#troubleshooting).
