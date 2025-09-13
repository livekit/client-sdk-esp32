# Network Connect

A simple utility for establishing a network connection with a single function call, used for the [LiveKit ESP32 SDK](https://github.com/livekit/client-sdk-esp32) examples.

**Important**: this component is intended to reduce boilerplate in examples; it only handles basic use cases and and does not provide robust error handling. Therefore, use in production applications is not recommended.

## Supported interfaces

- [x] WiFi
- [ ] Ethernet

## Usage

1. Configure network connection method and credentials using _Kconfig_ (see below)
2. Include the _network_connect.h_ header
3. Invoke `network_connect()` in your application's main function

## Configuration

Network connection method and credentials (if applicable) are configured using _Kconfig_; configure interactively using _menuconfig_, or manually add configuration to your application's _sdkconfig_ file.
