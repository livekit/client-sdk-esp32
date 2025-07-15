<!-- BEGIN_BANNER_IMAGE --><!-- END_BANNER_IMAGE -->

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

## Examples

This repository contains several examples you can use as a starting point for your project:
- [Voice agent](./examples/voice_agent/)
- *More examples coming soon*

See the README located in each example directory for setup information and build instructions.

## Documentation

Please refer to the [LiveKit Docs](https://docs.livekit.io/home/) for an introduction to the platform and its features, or to the API reference (*TODO: Not published yet*) for specifics about this SDK.


## Getting Help & Contributing

We invite you to join the [LiveKit Community Slack](https://livekit.io/join-slack) to get your questions answered, suggest improvements, or discuss how you can best contribute to this SDK.

<!-- BEGIN_REPO_NAV --><!-- END_REPO_NAV -->