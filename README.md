<!--BEGIN_BANNER_IMAGE-->

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/.github/banner_dark.png">
  <source media="(prefers-color-scheme: light)" srcset="/.github/banner_light.png">
  <img style="width:100%;" alt="The LiveKit icon, the name of the repository and some sample code in the background." src="https://raw.githubusercontent.com/livekit/client-sdk-esp32/main/.github/banner_light.png">
</picture>

<!--END_BANNER_IMAGE-->

# ESP32 SDK for LiveKit

[![API Reference](https://img.shields.io/badge/API_Reference-blue)](https://livekit.github.io/client-sdk-esp32/)
[![Component Registry](https://components.espressif.com/components/livekit/livekit/badge.svg)](https://components.espressif.com/components/livekit/livekit)
[![CI Status](https://img.shields.io/github/actions/workflow/status/livekit/client-sdk-esp32/ci.yml?label=CI)](https://github.com/livekit/client-sdk-esp32/actions/workflows/ci.yml)

Use this SDK to add realtime video, audio and data features to your ESP32 projects. By connecting to [LiveKit](https://livekit.io/) Cloud or a self-hosted server, you can quickly build applications such as multi-modal AI, live streaming, or video calls with minimal setup.

> [!IMPORTANT]
> This SDK is currently in Developer Preview mode and not ready for production use.
> There may be bugs and APIs are subject to change during this period.

## Features

- **Supported chips**: ESP32-S3 and ESP32-P4
- **Bidirectional audio**: Opus encoding, acoustic echo cancellation (AEC)
- **Video publishing**: H.264 encoding, subscribing coming soon
- **AI Agents**: interact with agents in the cloud built with [LiveKit Agents](https://docs.livekit.io/agents/)
- **Real-time data**: data packets, remote method calls (RPC)

## Getting Started

Before you begin, please ensure you have ESP IDF v5.4 or higher installed on your system (see installation guide for [macOS/Linux](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/linux-macos-setup.html) or [Windows](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/windows-setup.html)).

### Examples

One of the best ways to get started with LiveKit is by reviewing the [examples](./components/livekit/examples/) and choosing one as a starting point for your project:

- [**Voice AI Agent**](./components/livekit/examples/voice_agent/README.md): Conversational AI voice agent that interacts with hardware based on user requests.
- [**Minimal**](./components/livekit/examples/minimal/README.md): Basic example of connecting to a LiveKit room with bidirectional audio.
- [**Minimal Video**](./components/livekit/examples/minimal_video/README.md): Equivalent to the minimal example with video publishing.

Once you have chosen an example to be your starting point, create a fresh project from it locally using the following command:

```sh
idf.py create-project-from-example "livekit/livekit=0.3.2:<example>"
```

Substitute *\<example\>* for the example's directory name.

### Installation

If you would like to add LiveKit to your existing application, add it as a dependency using IDF:

```sh
idf.py add-dependency "livekit/livekit=0.3.2"
```

> [!IMPORTANT]
> Be sure to pin to a specific version as shown in the command above, as subsequent v0.x.x releases may have breaking changes.

## API Overview

With LiveKit added as a dependency to your application, include the LiveKit header and invoke
`livekit_system_init` early in your application's main function:

```c
#include "livekit.h"

void app_main(void)
{
    livekit_system_init();
    // Your application code...
}
```

### Configure media pipeline

LiveKit for ESP32 puts your application in control of the media pipeline; your application configures a capturer and/or renderer and provides their handles when creating a room.

#### Capturer: input from camera/microphone

- Required for rooms which will publish media tracks
- Created using the Espressif [*esp_capture*](https://components.espressif.com/components/espressif/esp_capture/) component
- Capture audio capture over I2S, video from MIPI CSI or DVI cameras
- After configuration, you will provide the `esp_capture_handle_t` when creating a room

#### Renderer: output to display/speaker

- Required for rooms which will subscribe to media tracks
- Created using the Espressif [*av_render*](https://components.espressif.com/components/tempotian/av_render/) component
- Playback audio over I2S, video on LCD displays supported by *esp_lcd*
- After configuration, you will provide the `av_render_handle_t` when creating a room

### Create room

Create a room object, specifying your capturer, renderer, and handlers for room events:

```c
static livekit_room_handle_t room_handle = NULL;

livekit_room_options_t room_options = {
    .publish = {
        .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
        .audio_encode = {
            .codec = LIVEKIT_AUDIO_CODEC_OPUS,
            .sample_rate = 16000,
            .channel_count = 1
        },
        .capturer = my_capturer
    },
    .subscribe = {
        .kind = LIVEKIT_MEDIA_TYPE_AUDIO,
        .renderer = my_renderer
    },
    .on_state_changed = on_state_changed,
    .on_participant_info = on_participant_info
};
if (livekit_room_create(&room_handle, &room_options) != LIVEKIT_ERR_NONE) {
    ESP_LOGE(TAG, "Failed to create room object");
}
```

This example does not show all available fields in room options—please refer to the [API reference](https://livekit.github.io/client-sdk-esp32/group__Lifecycle.html#structlivekit__room__options__t)
for an extensive list.

Typically, you will want to create the room object early in your application's lifecycle, and connect/disconnect as necessary based on user interaction.

### Connect room

With a room room handle, connect by providing a server URL and token:

```c
livekit_room_connect(room_handle, "<your server URL>", "<token>");
```

The connect method is asynchronous; use your `on_state_changed` handler provided in room options
to get notified when the connection is established or fails (e.g. due to an expired token, etc.).

Once connected, media exchange will begin:

1. If a capturer was provided, video and/or audio tracks will be published.
2. If a renderer was provided, the first video and/or audio tracks in the room will be subscribed to.

### Real-time data

In addition to real-time audio and video, LiveKit offers several methods for exchange real-time data between participants in a room.

#### Remote method call (RPC)

Define an RPC handler:

```c
static void get_cpu_temp(const livekit_rpc_invocation_t* invocation, void* ctx)
{
    float temp = board_get_temp();
    char temp_string[16];
    snprintf(temp_string, sizeof(temp_string), "%.2f", temp);
    livekit_rpc_return_ok(temp_string);
}
```

Register the handler on the room to allow it to be invoked by remote participants:

```c
livekit_room_rpc_register(room_handle, "get_cpu_temp", get_cpu_temp);
```

> [!TIP]
> In the [*voice_agent*](./examples/voice_agent/) example, RPC is used to allow an AI agent to interact
> with hardware by defining a series of methods for the agent to invoke.

#### User packets

Publish a user packet containing a raw data payload under a specific topic:

```c
const char* command = "G5 I0 J3 P0 Q-3 X2 Y3";

livekit_payload_t payload = {
     .bytes = (uint8_t*)command,
     .size = strlen(command)
};
livekit_data_publish_options_t options = {
    .payload = &payload,
    .topic = "gcode",
    .lossy = false,
    .destination_identities = (char*[]){ "printer-1" },
    .destination_identities_count = 1
};
livekit_room_publish_data(room_handle, &options);
```

## Documentation

Please refer to the [LiveKit Docs](https://docs.livekit.io/home/) for an introduction to the platform and its features. For more detail about the APIs available in this SDK, please refer to the [API Reference](https://livekit.github.io/client-sdk-esp32/).

## Known Issues

- In some cases, a remote participant leaving the room can lead to a disconnect.

## Getting Help & Contributing

We invite you to join the [LiveKit Community Slack](https://livekit.io/join-slack) to get your questions answered, suggest improvements, or discuss how you can best contribute to this SDK.

<!--BEGIN_REPO_NAV-->
<br/><table>
<thead><tr><th colspan="2">LiveKit Ecosystem</th></tr></thead>
<tbody>
<tr><td>LiveKit SDKs</td><td><a href="https://github.com/livekit/client-sdk-js">Browser</a> · <a href="https://github.com/livekit/client-sdk-swift">iOS/macOS/visionOS</a> · <a href="https://github.com/livekit/client-sdk-android">Android</a> · <a href="https://github.com/livekit/client-sdk-flutter">Flutter</a> · <a href="https://github.com/livekit/client-sdk-react-native">React Native</a> · <a href="https://github.com/livekit/rust-sdks">Rust</a> · <a href="https://github.com/livekit/node-sdks">Node.js</a> · <a href="https://github.com/livekit/python-sdks">Python</a> · <a href="https://github.com/livekit/client-sdk-unity">Unity</a> · <a href="https://github.com/livekit/client-sdk-unity-web">Unity (WebGL)</a> · <b>ESP32</b></td></tr><tr></tr>
<tr><td>Server APIs</td><td><a href="https://github.com/livekit/node-sdks">Node.js</a> · <a href="https://github.com/livekit/server-sdk-go">Golang</a> · <a href="https://github.com/livekit/server-sdk-ruby">Ruby</a> · <a href="https://github.com/livekit/server-sdk-kotlin">Java/Kotlin</a> · <a href="https://github.com/livekit/python-sdks">Python</a> · <a href="https://github.com/livekit/rust-sdks">Rust</a> · <a href="https://github.com/agence104/livekit-server-sdk-php">PHP (community)</a> · <a href="https://github.com/pabloFuente/livekit-server-sdk-dotnet">.NET (community)</a></td></tr><tr></tr>
<tr><td>UI Components</td><td><a href="https://github.com/livekit/components-js">React</a> · <a href="https://github.com/livekit/components-android">Android Compose</a> · <a href="https://github.com/livekit/components-swift">SwiftUI</a> · <a href="https://github.com/livekit/components-flutter">Flutter</a></td></tr><tr></tr>
<tr><td>Agents Frameworks</td><td><a href="https://github.com/livekit/agents">Python</a> · <a href="https://github.com/livekit/agents-js">Node.js</a> · <a href="https://github.com/livekit/agent-playground">Playground</a></td></tr><tr></tr>
<tr><td>Services</td><td><a href="https://github.com/livekit/livekit">LiveKit server</a> · <a href="https://github.com/livekit/egress">Egress</a> · <a href="https://github.com/livekit/ingress">Ingress</a> · <a href="https://github.com/livekit/sip">SIP</a></td></tr><tr></tr>
<tr><td>Resources</td><td><a href="https://docs.livekit.io">Docs</a> · <a href="https://github.com/livekit-examples">Example apps</a> · <a href="https://livekit.io/cloud">Cloud</a> · <a href="https://docs.livekit.io/home/self-hosting/deployment">Self-hosting</a> · <a href="https://github.com/livekit/livekit-cli">CLI</a></td></tr>
</tbody>
</table>
<!--END_REPO_NAV-->