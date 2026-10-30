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

For ESP32-S31-Korvo-1 or ESP32-S31-Function-CoreBoard-1, use ESP-IDF 6.1 and
configure the preview target first:

```sh
idf.py --preview set-target esp32s31
idf.py --preview menuconfig
idf.py --preview build
idf.py --preview -p PORT -b 921600 flash monitor
```

The S31 defaults select the Korvo-1 ES8389 audio codec and a 16 MB flash
layout. To build for the Function-CoreBoard-1 instead, set:

```ini
CONFIG_LK_EXAMPLE_CODEC_BOARD_TYPE="ESP32_S31_FUNCTION_COREBOARD_1"
```

The Function-CoreBoard uses its mono ES8311 codec, onboard microphone, and J9
speaker connector. The Korvo-1 uses its ES8389 codec and separate left/right
speaker connectors. Do not flash a build configured for one board onto the
other and expect audio to work; their codec types and audio GPIOs differ.
Without AEC, Korvo capture selects the left input before mono conversion and
uses 30 dB microphone gain while preserving the shared 48 kHz stereo bus.
This avoids averaging the raw slots; it does not change speaker settings or
codec reference routing.
Both profiles expose their single WS2812 RGB LED to the agent (GPIO 60 on the
Function-CoreBoard-1 and GPIO 37 on the Korvo-1). The included agent queries
the firmware for its board identity before greeting, so it does not advertise
unimplemented peripherals from the other S31 board or the older S3 board.

See the [S31 setup and verification guide](../../../../docs/esp32s31.md) for
the required ESP-IDF patch, board wiring, known limitations, and host-side
interoperability test.

Once running, the example will establish a network connection, connect to a LiveKit room, and print the following message:

```txt
I (19508) livekit_example: Room state: Connected
```

If you are using the provided Sandbox, you should be able to converse with the agent at this point. Start by asking "What's the CPU temperature?"

## Local agent using LiveKit Inference

Function-CoreBoard-1 requires AEC to prevent the agent from hearing its own
speaker replies. `CONFIG_LK_EXAMPLE_ENABLE_AEC=y` is the default for this
example on both S31 boards. Keep it enabled when using the board speaker.
Both boards passed sustained conversation and overlapping-speech listening
checks with the standalone AEC source.

For your own Cloud project, the Sandbox token server provides room access;
a voice agent must also be running. The bundled Python agent supports
LiveKit Inference using `LIVEKIT_URL`, `LIVEKIT_API_KEY`, and
`LIVEKIT_API_SECRET` in `agent/.env.local`, without a separate provider key.
Copy `agent/.env.example` to `agent/.env.local` and fill in your project credentials.
From the `agent` directory, run this PowerShell command for a hardware
test (replace the room name with the firmware's configured room):

```powershell
uv run python agent.py connect --room esp32-s31-interop
```

The locked CLI generates an agent identity when none is supplied. If you
set `--participant-identity`, choose an identity different from the board's;
this option names the agent, not the board it should control. Sharing an
identity disconnects the previous participant. The locked `connect` command
does not accept `--no-watch`.

The agent follows the v0.3.11 upstream example: AssemblyAI Universal-3.5 Pro
STT, Gemma 4, and Fish Audio S2.1 Pro TTS through LiveKit Inference, with
adaptive interruptions and the Inference turn detector. Set
`LIVEKIT_AGENT_VOICE_ID` in `agent/.env.local` to choose a Fish Audio voice;
the default is `fa4c9eb3dccc4806b382b40d61c6b10a`. The earlier
`LIVEKIT_AGENT_BACKEND` selector is no longer used. Upstream replaced its
OpenAI Realtime example with this Inference pipeline in v0.3.11; the refresh
follows that upstream change.

Board discovery runs before the greeting, and the session and hardware RPCs
use the same participant. RGB commands set the board's single pixel once
and leave it on until an explicit off request. Console mode uses simulated
hardware tools and does not wait for a board.

The v0.3.11 refresh passed attended conversation, board identity, temperature
and RGB control checks on both S31 boards. Korvo also resumed conversation
after an agent restart without a board reset. Detailed overlap, adaptive
interruptions and resistance to self-triggered replies were not separately
assessed in this refresh; the earlier sustained listening results used a
different Inference pipeline. See the [refresh validation record](../../../../docs/s31-refresh-validation.md)
for the tested scope. Stop the local agent with Ctrl+C and disconnect the
board after testing so microphone publication does not continue unattended.

## Next Steps

Explore how the agent is built (see its source in the *./agent* directory). If you are unfamiliar with [LiveKit Agents](https://docs.livekit.io/agents/), refer to the [Voice AI Quickstart](https://docs.livekit.io/agents/start/voice-ai/) to learn how you can build upon the example agent or create your own from scratch.
