# S31 audio renderer lifecycle tests

These host tests compile the actual `s31_audio_render.c` against a small
threaded fake of FreeRTOS and the codec. They need CMake and a C11 compiler
with threads and atomics (for example, recent GCC or Visual Studio 2022).
They do not need ESP-IDF, a board, network access, or LiveKit credentials.

From the repository root:

```sh
cmake -S tools/s31-audio-test -B tools/s31-audio-test/build
cmake --build tools/s31-audio-test/build --config Debug
ctest --test-dir tools/s31-audio-test/build -C Debug --output-on-failure
```

Add `--repeat until-fail:10` to the last command to repeat the threaded tests.
Assertions remain enabled even in a release build.

Coverage includes:

- Initial prefill, short clips without EOS, EOS, and restart after idle.
- Re-priming after brief queue starvation and extending the short-tail
  deadline while new data arrives.
- Frames larger than the ring, ring wrap, and byte-exact PCM ordering.
- Invalid/unaligned frames and empty EOS.
- Close during an active codec write and a blocked producer.
- Discarding old PCM before reopening a replacement track.
- Codec failure, task creation failure, and volume-setting failure.
- Bounded producer timeout when the consumer is stalled.
- Repeated open/close, volume reapplication, and worker cleanup.

The fake is deliberately not a hardware simulator. Passing does not verify
FreeRTOS scheduling on S31, DMA timing, I2S clocks, electrical behavior, Opus,
network jitter, or audible quality. Those still require the device tests.
Close waits for the codec call to return; it does not force-delete a task if
the underlying driver hangs.
