# S31 AEC capture source host tests

<!-- cspell:words DESP -->

This test uses fake codec and AEC implementations to check capture format
negotiation, separate mic/reference filtering, frame buffering, timestamps,
allocation failure handling and repeated stop/start. It does **not** measure
acoustic suppression or validate the board's physical reference routing.

Silent-startup regressions verify that initial all-zero chunks preserve silence
and timestamps without entering AEC, that the first nonzero chunk starts AEC,
and that subsequent silent chunks continue through AEC. Restarting restores
the initial unprimed state. This covers the integration's protection against
esp-sr 2.5.2 all-zero startup saturation (reproduced with FD_HIGH_PERF).
The production mode is VOIP_HIGH_PERF; the fake library verifies the guard's
behavior, not whether that mode exhibits the same library defect.

Configure with the installed `espressif/esp_capture` component headers:

```sh
cmake -S tools/s31-aec-test -B build-aec-test -DESP_CAPTURE_COMPONENT_DIR=/absolute/path/to/espressif__esp_capture
cmake --build build-aec-test --config Debug
ctest --test-dir build-aec-test -C Debug --output-on-failure
```

Hardware validation must verify far-end echo suppression and intelligible
near-end speech, including overlapping speech, before enabling AEC by default.

## Codec layout coverage

The suite runs the same capture/lifecycle checks for ES8311's two-word input
and ES8389's four-word DMA input. Distinct values in all four ES8389 words
verify selection of mic L (word 1) and DAC L reference (word 0), without
accidentally feeding the other microphone or DAC channel into AEC.
