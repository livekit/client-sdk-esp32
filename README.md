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
> This SDK is currently in Developer Preview mode and not ready for production use.
> There will be bugs and APIs may change during this period.

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

Please be sure to pin to a specific version tag as subsequent 0.x.x releases may have breaking changes. In the future, this SDK will be added to the [ESP component registry](https://components.espressif.com).

## Examples

One of the best ways to get started with LiveKit is by reviewing the examples and choosing one as a starting point for your project:

- [Voice Agent](./examples/voice_agent/README.md): conversational AI voice agent that interacts with hardware based on user requests.
- *More examples coming soon*

## Documentation

Please refer to the [LiveKit Docs](https://docs.livekit.io/home/) for an introduction to the platform and its features, or see the [API Reference](https://livekit.github.io/client-sdk-esp32/) for specifics about this SDK.

## Known Issues

- In some cases, a remote participant leaving the room can lead to a disconnect.

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