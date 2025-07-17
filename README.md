<!--BEGIN_BANNER_IMAGE-->
<picture>
    <source media="(prefers-color-scheme: dark)" srcset="/.github/banner_dark.png">
    <source media="(prefers-color-scheme: light)" srcset="/.github/banner_light.png">
    <img style="width:100%;" alt="The LiveKit icon, the name of the repository and some sample code in the background." src="https://raw.githubusercontent.com/livekit/client-sdk-esp32/main/.github/banner_light.png">
</picture>
<!--END_BANNER_IMAGE-->

# ESP-32 SDK for LiveKit

Use this SDK to add realtime video, audio and data features to your ESP-32 projects. By connecting to [LiveKit](https://livekit.io/) Cloud or a self-hosted server, you can quickly build applications such as multi-modal AI, live streaming, or video calls with minimal setup.

> [!WARNING]
> This SDK is in the early stages of development and may undergo breaking changes.

## Features

- **Supported chipsets**: ESP32-S3 and ESP32-P4
- **Bidirectional audio**: Opus encoding, acoustic echo cancellation (AEC)
- **Bidirectional video**: *coming soon*
- **Real-time data**: data packets, remote method calls (RPC)

## Installation

In your application's IDF component manifest, add LiveKit as a Git dependency:

```yaml
dependencies:
  livekit:
    git: https://github.com/livekit/client-sdk-esp32.git
    path: components/livekit
    version: <current version tag>
```

In the future, this SDK will be added to the [ESP component registry](https://components.espressif.com).

## Documentation

Please refer to the [LiveKit Docs](https://docs.livekit.io/home/) for an introduction to the platform and its features, or to the API reference (*TODO: Not published yet*) for specifics about this SDK.

## Example: Voice Agent

This repository contains an example application built with this SDK and [LiveKit Agents](https://docs.livekit.io/agents/), enabling bidirectional voice communication with an AI agent. The agent can interact with hardware in response to user requests. Below is an example of a conversation between a user and the agent:

> **User:** What is the current CPU temperature? \
> **Agent:** The CPU temperature is currently 33°C.

> **User:** Turn on the blue LED. \
> **Agent:** *[turns blue LED on]*

> **User:** Turn on the yellow LED. \
> **Agent:** I'm sorry, the board does not have a yellow LED.

### Requirements

- Software:
    - [IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) release v5.4 or later
    - Python 3.9 or later
    - LiveKit Cloud Project
    - Sandbox Token Server (created from your cloud project)
    - API keys for OpenAI, Deepgram, and Cartesia.
- Hardware
    - Dev board: [ESP32-S3-Korvo-2](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/dev-boards/user-guide-esp32-s3-korvo-2.html)
    - Two micro USB cables: one for power, one for flashing
    - Mono enclosed speaker (example from [Adafruit](https://www.adafruit.com/product/3351))

### Run example

To run the example on your board, begin in your terminal by navigating to the example's root directory: *[examples/voice_agent](./examples/voice_agent/)*.

#### 1. Configuration

The example requires a network connection and Sandbox ID from your [LiveKit Cloud Project](https://cloud.livekit.io/projects/p_/sandbox/templates/token-server). To configure these settings from your terminal, launch *menuconfig*:
```sh
idf.py menuconfig
```

With *menuconfig* open, navigate to the *LiveKit Example* menu and configure the following settings:

- Network → Wi-Fi SSID
- Network → Wi-Fi password
- Room connection → Sandbox ID

For more information about available options, please refer to [this guide](./examples/README.md#configuration).

#### 2. Build & flash

Begin by connecting your dev board via USB. With the board connected, use the following command
to build the example, flash it to your board, and monitor serial output:

```sh
idf.py flash monitor
```

Once running on device, the example will establish a network connection and then connect to a
LiveKit room. Once connected, you will see the following log message:

```sh
I (19508) livekit_example: Room state: connected
```

If you encounter any issues during this process, please refer to the example [troubleshooting guide](./examples/README.md/#troubleshooting).

### Run agent

With the example running on your board, the next step is to run the agent so it can join the room.
Begin by navigating to the agent's source directory in your terminal: *[examples/voice_agent/agent](./examples/voice_agent/agent)*.

In this directory, create a *.env* file containing the required API keys:

```sh
DEEPGRAM_API_KEY=<your Deepgram API Key>
OPENAI_API_KEY=<your OpenAI API Key>
CARTESIA_API_KEY=<your Cartesia API Key>
LIVEKIT_API_KEY=<your API Key>
LIVEKIT_API_SECRET=<your API Secret>
LIVEKIT_URL=<your server URL>
```

With the API keys in place, download the required files and run the agent in development mode as follows:

```sh
python agent.py download-files
python agent.py dev
```

With the agent running, it will discover and join the room, and you will now be able to engage in two-way conversation. Try asking some of the questions shown above.

## Getting Help & Contributing

We invite you to join the [LiveKit Community Slack](https://livekit.io/join-slack) to get your questions answered, suggest improvements, or discuss how you can best contribute to this SDK.

<!--BEGIN_REPO_NAV-->
<br/><table>
<thead><tr><th colspan="2">LiveKit Ecosystem</th></tr></thead>
<tbody>
<tr><td>LiveKit SDKs</td><td><a href="https://github.com/livekit/client-sdk-js">Browser</a> · <a href="https://github.com/livekit/client-sdk-swift">iOS/macOS/visionOS</a> · <a href="https://github.com/livekit/client-sdk-android">Android</a> · <a href="https://github.com/livekit/client-sdk-flutter">Flutter</a> · <a href="https://github.com/livekit/client-sdk-react-native">React Native</a> · <a href="https://github.com/livekit/rust-sdks">Rust</a> · <a href="https://github.com/livekit/node-sdks">Node.js</a> · <a href="https://github.com/livekit/python-sdks">Python</a> · <a href="https://github.com/livekit/client-sdk-unity">Unity</a> · <a href="https://github.com/livekit/client-sdk-unity-web">Unity (WebGL)</a> · <b>ESP-32</b></td></tr><tr></tr>
<tr><td>Server APIs</td><td><a href="https://github.com/livekit/node-sdks">Node.js</a> · <a href="https://github.com/livekit/server-sdk-go">Golang</a> · <a href="https://github.com/livekit/server-sdk-ruby">Ruby</a> · <a href="https://github.com/livekit/server-sdk-kotlin">Java/Kotlin</a> · <a href="https://github.com/livekit/python-sdks">Python</a> · <a href="https://github.com/livekit/rust-sdks">Rust</a> · <a href="https://github.com/agence104/livekit-server-sdk-php">PHP (community)</a> · <a href="https://github.com/pabloFuente/livekit-server-sdk-dotnet">.NET (community)</a></td></tr><tr></tr>
<tr><td>UI Components</td><td><a href="https://github.com/livekit/components-js">React</a> · <a href="https://github.com/livekit/components-android">Android Compose</a> · <a href="https://github.com/livekit/components-swift">SwiftUI</a></td></tr><tr></tr>
<tr><td>Agents Frameworks</td><td><a href="https://github.com/livekit/agents">Python</a> · <a href="https://github.com/livekit/agents-js">Node.js</a> · <a href="https://github.com/livekit/agent-playground">Playground</a></td></tr><tr></tr>
<tr><td>Services</td><td><a href="https://github.com/livekit/livekit">LiveKit server</a> · <a href="https://github.com/livekit/egress">Egress</a> · <a href="https://github.com/livekit/ingress">Ingress</a> · <a href="https://github.com/livekit/sip">SIP</a></td></tr><tr></tr>
<tr><td>Resources</td><td><a href="https://docs.livekit.io">Docs</a> · <a href="https://github.com/livekit-examples">Example apps</a> · <a href="https://livekit.io/cloud">Cloud</a> · <a href="https://docs.livekit.io/home/self-hosting/deployment">Self-hosting</a> · <a href="https://github.com/livekit/livekit-cli">CLI</a></td></tr>
</tbody>
</table>
<!--END_REPO_NAV-->