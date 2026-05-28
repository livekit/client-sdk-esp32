# Host-side tests

A small CMake project that compiles a narrow slice of the SDK for Linux
and runs unit tests as part of CI, without needing an ESP32 board.

## What runs here

Currently the only test target is `test_extract_attributes`, which
exercises `protocol_data_packet_extract_attributes` — the manual
second-pass parser used to decode the RPC v2 attributes map out of a
`DataPacket` wire-format buffer. (nanopb's auto-allocated submessages
cannot have decode callbacks installed externally, so this parser
walks the raw bytes itself; testing it on host is much cheaper than
flashing an ESP32.)

## What does NOT run here yet

The full `rpc_client_manager` / `rpc_server_manager` tests
(`test_app/main/test_rpc.c`) depend on FreeRTOS `xTimer*` and
`xSemaphore*` primitives, plus `esp_timer`. Bringing those up under
Linux would require either a FreeRTOS POSIX port or a fresh set of
shims, neither of which is wired up here. Those tests continue to run
on real ESP32 hardware via `pytest-embedded`.

## Local invocation

```
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## CI

`.github/workflows/host_test.yml` runs the above on every PR. It is a
build-and-test step that complements the existing IDF cross-compile
build in `build.yml`.
