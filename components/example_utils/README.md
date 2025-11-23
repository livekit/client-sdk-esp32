# LiveKit Example Utilities

Common functionality required for example applications, used by the [LiveKit ESP32 SDK](https://github.com/livekit/client-sdk-esp32) for its examples.

**Important**: this component is intended to reduce boilerplate in examples; it only handles basic use cases and and does not provide robust error handling. Therefore, use in production applications is not recommended.

## Network Connection

Establish a network connection with a single function call, configured via _Kconfig_.

### Supported Interfaces

- [x] WiFi
- [x] Ethernet (ESP32-P4 only)

### Usage

1. Configure network connection method and credentials using _Kconfig_
2. Include the _livekit_example_utils.h_ header
3. Invoke `lk_example_network_connect()` in your application's main function
