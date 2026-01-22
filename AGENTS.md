# Instructions for AI Agents

Overview of the LiveKit ESP32 SDK and the structure of this repository.

## Components

- The sources for the LiveKit ESP32 SDK itself and supporting components live in the `components/` directory.
- All components are published to the [IDF Component Registry](https://components.espressif.com) for easy installation in projects.
- Components in `third_party/` are vendored third-party dependencies and are not relevant for using the SDK.

## Examples

- Example applications built with the SDK live in `components/livekit/examples/`.
- See the README in each example directory for specific instructions and hardware requirements.
- Most users will want to begin with one of the examples.
- Examples are also uploaded to the IDF Component Registry which allows them to be bootstrapped with `idf.py create-project-from-example "livekit/livekit=<current version>:<example>"`, where `<current version>` is the latest release of the SDK and `<example>` is the directory name the example lives in.

## Requirements

- To build applications, [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/index.html) must be installed on the user's system.
- The recommended way to install IDF is by using [EIM](https://docs.espressif.com/projects/idf-im-ui/en/latest/), the ESP-IDF installation manager.
