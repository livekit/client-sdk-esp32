# ESP32 interoperability smoke test

This test joins a LiveKit room from Node.js and verifies the ESP32 voice-agent
example's RPC, reliable/lossy packet, and text/byte stream paths.

Add `--media-only` when testing an example such as `minimal` that does not
enable the test-firmware RPC/data endpoints. This skips RPC, packet, stream,
LED, and device-memory checks while retaining participant discovery and the
selected audio checks.

On the ESP32, enable `LiveKit Example > Enable data/RPC interoperability test
endpoints`, set the room to `esp32-interop`, and set the participant name to
`esp32-device`. Build, flash, and wait for the room state to become `Connected`.

On the host:

```sh
cd tools/interop-test
npm install
export LIVEKIT_TOKEN_SERVER_ID='<development-token-server-id>'
node ./interop-test.mjs
```

In PowerShell, set the variable with
`$env:LIVEKIT_TOKEN_SERVER_ID = '<development-token-server-id>'`.

Add `--audio` to also receive and decode 25 frames from the ESP32 microphone
track and publish a two-second tone for the ESP32 render path. The host
can verify transport and subscription automatically; confirming audible output
still requires a connected speaker and a listener near the board.

Add `--disable-dtx` to disable Opus discontinuous transmission on host-published
audio tracks, including both burst and repeated-track tests. Without this flag,
the SDK publication default is retained. This does not change the board's
microphone encoding. Record the chosen setting when comparing playback tests.

Use `--capture-only` to subscribe briefly to the ESP32 microphone and validate
25 decoded Opus frames without publishing a speaker-test track. Captured audio
is measured in memory and is neither saved nor played by default.
With the speaker's consent, add `--capture-wav path.wav` to save received
16 kHz mono PCM for intelligibility checks. The destination must not already
exist; this option does not play the recording.
Use `--capture-frames 500` for an approximately five-second hardware signal
check. A peak below 32 is treated as effectively silent rather than reported as
a successful microphone test.

Use `--audio --full-duplex --capture-frames 500` to capture the ESP32 microphone
while the ESP32 concurrently renders the host tone. This specifically checks a
board's shared-codec/shared-I2S full-duplex path instead of running the capture
and render checks sequentially.

The current default tone is 220 Hz mono at 20% of digital full scale. Override
it with `--tone-hz 180`, set `--tone-level` from greater than 0 through 1, and
select `--tone-channel mono|left|right|both`. For example, this sends three
two-second bursts only in the left channel:

```sh
node ./interop-test.mjs --audio-bursts 3 --tone-hz 180 --tone-level 0.8 --tone-channel left
```

Add `--render-only` to publish the synthetic tone without subscribing to or
decoding the ESP32 microphone. In this mode the host joins with automatic track
subscription disabled. This is useful for speaker-only checks where microphone
audio must not be sent to the test host.

This does not mute the firmware microphone: the device can still publish it
to the room. Disconnect other participants or leave the room when testing is
finished. The host disconnects in its cleanup path, but it does not disconnect
the board. A transport pass is not an acoustic-quality or DMA-underrun test.

Use `--tone-gap-ms 1500` to insert an explicit 1.5-second silent gap between
bursts. The default is zero so sustained soak tests remain continuous.
The harness waits for LiveKit's local audio-source queue to drain before it
unpublishes the track so the final queued burst is not truncated.

The sustained-burst test also sends two seconds of silence after LiveKit first
reports the track subscribed. This gives the ESP32 subscriber time to complete
DTLS, decoder, codec, and I2S startup before the first audible sample. Override
it with `--tone-warmup-ms 0..10000` when diagnosing startup timing.

Use `--tone-duration-ms 100..30000` to change each tone's duration. Two seconds
of transmitted silence are appended after the last tone by default so remote
buffers drain before unpublish; override that with `--tone-postroll-ms`.

Use `--audio-bursts N` for a sustained render test. It publishes one audio
track, sends `N` consecutive two-second tone bursts, and unpublishes only after
the final burst. This avoids confusing media stability with repeated SDP
renegotiation.

`--audio-iterations N` is a separate track-lifecycle stress test: it publishes,
renders, and unpublishes a new track on every iteration. Twelve iterations,
including two-digit SDP media IDs, have been hardware-verified with `esp_peer`
1.5.4. The peer still accumulates media sections during this pattern, so use
`--audio-bursts` for long media soaks and reserve `--audio-iterations` for
renegotiation testing. Do not combine the two options.

The harness reads internal-RAM and PSRAM statistics from the test firmware
before and after every run and reports the free-memory delta and largest
remaining blocks. Passing `--audio` alone runs one sustained-render burst.

The token server ID may instead be supplied with `--token-server-id`. Room,
peer identity, device identity, and timeout can be overridden with `--room`,
`--identity`, `--device`, and `--timeout-ms` respectively.

The firmware endpoints are test-only and default to disabled.

Add `--led-cycle` to request red, green, blue, and white for one second each,
then turn the board's single RGB pixel off. Successful RPC responses verify the
control path; visually confirm the colors and final off state on the board.

Use `--wifi-cycles N` to make the test firmware stop Wi-Fi for five seconds,
then verify that Wi-Fi, signaling, the LiveKit room, RPC, and the reliable data
channel recover. Override the outage with `--wifi-outage-ms 1000..30000`; use a
`--timeout-ms` long enough for the selected outage plus signaling recovery.
This RPC is available only when the example uses Wi-Fi and the interoperability
endpoints are enabled.
