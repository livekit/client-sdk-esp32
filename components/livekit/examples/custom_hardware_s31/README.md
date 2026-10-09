# Custom Hardware: ESP32-S31

This example is a worked ESP32-S31 adaptation of the sibling
[`custom_hardware`](../custom_hardware/README.md) example. It connects to a
LiveKit room with bidirectional Opus audio while manually initializing the
board's I2C bus, I2S channels, audio codec, and speaker amplifier.

Unlike the `minimal` and `voice_agent` examples, this example intentionally
initializes its own hardware instead of calling `livekit_s31_board_init()`.
It reuses only the buffered audio renderer from `livekit/s31_board`. The duplicated
hardware setup is educational: it shows the code an application author needs
when adapting an S31 design that does not yet have a board-support component.

## Supported hardware

- **ESP32-S31-Korvo-1**: ES8389 codec, onboard microphones, and two speaker
  connectors. The example renders stereo PCM so either speaker connector can
  be used.
- **ESP32-S31-Function-CoreBoard-1**: ES8311 codec, onboard mono microphone,
  NS4150B amplifier, and J9 speaker connector. Physical audio is mono; the
  shared I2S bus uses stereo slots, with conversion in the media pipeline.

The board implementations are deliberately kept next to the application:

- `main/board_korvo.c` contains the Korvo-1 pin and ES8389 setup.
- `main/board_function_core.c` contains the Function-CoreBoard-1 pin, ES8311,
  and amplifier setup.
- `main/board.c` selects one implementation behind the interface consumed by
  the LiveKit media code.

## Requirements

- ESP-IDF 6.1 with the ESP32-S31 preview target enabled.
- An S31 board with PSRAM and at least 8 MB of flash; the supplied official
  boards have 16 MB.
- A compatible passive speaker connected to the selected board's speaker
  output.
- A 2.4 GHz Wi-Fi network and a LiveKit Sandbox token server ID, or a
  pregenerated server URL and participant token.

## Configure

Open an ESP-IDF 6.1 shell and select the preview target:

```sh
idf.py --preview set-target esp32s31
idf.py --preview menuconfig
```

In **LiveKit Example**, select either **ESP32-S31-Korvo-1** or
**ESP32-S31-Function-CoreBoard-1**, configure the speaker volume, and select a
room authentication method. Configure Wi-Fi under **LiveKit Example
Utilities**.

For the Function Core profile, the alternative defaults file can be applied
without menuconfig:

```sh
idf.py --preview -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.function_core" build
```

Do not commit a generated `sdkconfig` containing a Wi-Fi password or LiveKit
token.

## Build, flash, and monitor

Replace `PORT` with the board's serial port:

```sh
idf.py --preview build
idf.py --preview -p PORT -b 921600 flash monitor
```

The firmware initializes the selected codec, connects to Wi-Fi, obtains a
LiveKit token, and joins a room. A successful connection includes:

```text
Room state changed: CONNECTED
```

Connect another participant or voice agent to verify microphone publication
and speaker playback.

The Korvo capture path uses 30 dB microphone gain and duplicates the left
microphone slot before mono conversion. This preserves the shared 48 kHz
stereo bus while avoiding cancellation from averaging the raw input slots,
matching the minimal, voice-agent, and minimal-video examples.

Function-CoreBoard uses separate ADC and DAC interfaces for the same ES8311
chip. This lets the codec driver retain microphone clocks when a speaker
track closes, while disabling the speaker amplifier. The interfaces share
the board's I2C and I2S buses; only the DAC interface owns the amplifier pin.

## Optional audio diagnostics

Boot-time wiring diagnostics are disabled by default. Enable **speaker channel
self-test** in `menuconfig` to play a 180 Hz tone. Korvo-1 plays left, right,
and both-channel bursts; Function-CoreBoard-1 plays a mono burst.

Codec volume and generated PCM level are separate settings. For a controlled
comparison with the network tone, set **Audio self-test volume** and the normal
**speaker volume** to the same value, then match **Audio self-test PCM level**
to the host's `--tone-level` (80% corresponds to `0.8`). Begin at a comfortable
level. Do not change either setting, speaker position, or wiring between
comparison runs. The standalone diagnostic bypasses Opus and LiveKit; normal
room playback uses the shared buffered renderer.

On Function-CoreBoard-1, **microphone level self-test** plays a short cue,
captures five seconds, and reports peak and RMS levels before playing a longer
stop cue. Captured samples are neither stored nor transmitted. The diagnostics
run before LiveKit initializes its normal media pipeline.

For automated clean configuration and compilation from the repository root,
use `idf-build-apps` from an ESP-IDF 6.1 environment:

```sh
idf-build-apps build --recursive --target esp32s31 --enable-preview-targets
```

## Intent and limitations

This is an audio hardware-porting example, not a second production board
abstraction. New applications using either official board should normally use
`livekit/s31_board` through `minimal` or `voice_agent` instead.

Video is outside this example. Acoustic echo cancellation is off by default and
is enabled with `CONFIG_LK_EXAMPLE_ENABLE_AEC=y`. The enabled path
uses `livekit_s31_aec_source_new()` with the selected board's microphone and
digital DAC-reference layout. It retains 48 kHz hardware operation and emits
16 kHz mono PCM. With AEC disabled, the example uses the audio-device capture
source. A new board port must establish its own reference layout before
enabling AEC.
