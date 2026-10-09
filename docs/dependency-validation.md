# Dependency and ESP-IDF compatibility validation

The dependency proposal is based on SDK v0.3.11. It retains esp_peer 1.5.5
and WebSocket 1.8.0 and pins capture to 1.0.2. It adds no S31-specific code.

## Validation on 2026-09-20

The combined dependency, generic-fix and S31-support candidate passed all
seven S31 example builds on ESP-IDF 6.1 with the documented S31 toolchain
patch. Every image fits its existing application partition. All 238 managed
component instances passed checksum verification.

That combined candidate also passed attended voice and board-control checks
on Korvo-1 and Function-CoreBoard-1, audio recovery after an agent restart
on Korvo-1, and the existing sustained Korvo camera/audio receiver test.
The latter received 6,294 video frames and 62,999 audio frames over
628.216 seconds of video, with a maximum video gap of 362 milliseconds.

These results exercise this dependency set with the other proposed changes;
they are not standalone S3/P4 verification. At that time, the standalone
ESP-IDF 5.4.4/5.5.3 S3/P4 CI matrix and registry dry-run action had not been
rerun. Previous CI results retain their original dependency versions and scope.

## Compatibility correction validation on 2026-10-09

Local testing applied the proposed correction to isolated copies of the
dependency branch at `3423cc7` and the combined S31 branch at `b685133`.
The unmodified dependency branch also reproduced all four upstream build
failures in the [fork's Build workflow](https://github.com/ibytergj/client-sdk-esp32/actions/runs/37880294947).
The fatal error was dependency resolution reporting `CODEC_UAC_SUPPORT`
from an unselected candidate. The IDF 6 certificate settings in shared
defaults produced separate unknown-option warnings on IDF 5.

The proposed correction makes these compatibility changes:

- Load the certificate defaults only on IDF 6. Both certificate settings
  remain enabled in the IDF 6 examples, including the S31 voice build.
- Pin `idf-build-apps` to 3.0.2, use component manager 2.4.11 on IDF 5 and
  3.1.2 on IDF 6, and add IDF 6.1 to the S3/P4 build matrix. The IDF Python
  dependency checks pass with those tool versions.
- Declare the voice example's temperature-sensor and JSON dependencies
  explicitly, selecting built-in JSON on IDF 5 and registry cJSON on IDF 6.
- Preserve the S3 voice example's 120 MHz flash setting with explicit IDF 6
  high-performance mode and a bootloader aware of dummy-cycle changes.
- On IDF 6.1/P4, use built-in remote Wi-Fi, select Hosted 2.12.13 for its
  explicit SDMMC driver dependency, and retain the earlier P4 chip revision.
  IDF 5 keeps its existing Hosted and external Wi-Fi dependency versions.
- Retain strict checks for SDK and test sources. On IDF 6, treat the IDF
  RISC-V CPU headers as system headers for the SDK's conversion checks and
  keep string-literal const diagnostics non-fatal only for the unmodified
  renderer, speech, capture and codec-board dependencies in the test app.
- Give the peer API writable static storage for its channel labels.
  The `_reliable` and `_lossy` values and channel behavior are unchanged.

Every application in the matrix below passed on both corrected candidates:

| ESP-IDF | Target | Applications per branch | Dependency branch | Combined S31 branch |
| --- | --- | --- | --- | --- |
| 5.4.4 | ESP32-S3 | 4 | Pass | Pass |
| 5.4.4 | ESP32-P4 | 2 | Pass | Pass |
| 5.5.3 | ESP32-S3 | 4 | Pass | Pass |
| 5.5.3 | ESP32-P4 | 2 | Pass | Pass |
| 6.1 | ESP32-S3 | 4 | Pass | Pass |
| 6.1 | ESP32-P4 | 2 | Pass | Pass |

The S3 set comprises `custom_hardware`, `minimal`, `voice_agent` and
`test_app`; the P4 set comprises `minimal_video` and `test_app`. This gives
18 application/version/target combinations per branch, 36 in total.
Initial Docker builds used fresh source and configuration trees. Follow-up
checks reused compiled dependencies; the final IDF 6.1/S3 dependency-branch
check explicitly regenerated configuration to verify the compiler options.
The corrected S31 Korvo voice example also rebuilt with the documented
IDF 6.1 toolchain patch and the released dependency constraints.

These are compilation checks, not new S3/P4 hardware results. The generated
IDF 6.1/P4 configuration retains C6 and the existing SDIO/reset pins, but the
new Hosted selection has not been exercised with the board's coprocessor
firmware. Moving an S3 board to the IDF 6 high-performance flash profile
requires flashing its matching bootloader; app-only OTA does not enable the
bootloader's dummy-cycle support. No boards were flashed during these checks.

The corrected dependency branch's [GitHub validation run](https://github.com/ibytergj/client-sdk-esp32/actions/runs/37921602872)
passed all six build jobs, covering its 18 application/version/target
combinations. The registry dry-run also passed. The tested application
sources, defaults and build commands match this compatibility correction.
The submitted pull request's checks will run again when its branch is updated.

The combined S31 candidate also passed its own [six-job GitHub build matrix](https://github.com/ibytergj/client-sdk-esp32/actions/runs/37927152386)
and registry dry-run. Seven fresh native S31 example/board builds passed
with the documented toolchain patch. The renderer and AEC host suites
passed ten repetitions each, and the existing offline agent tests and
lint/format checks passed. See `s31-refresh-validation.md` for their scope.
