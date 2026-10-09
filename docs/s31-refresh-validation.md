# S31 validation on SDK v0.3.11

## Candidate and build checks

Testing on 2026-09-20 used the combined dependency, generic-fix and S31
support candidate `7bf18ec`, based on upstream v0.3.11 (`fbf09ed`). Subsequent
closeout commits change documentation only; the tested firmware and agent
sources are unchanged.

Seven S31 example/board profiles built on ESP-IDF 6.1 with the documented
toolchain patch, and every image fits its partition. The audio and AEC host
suites passed. All 238 managed-component instances passed checksum checks.
The Python agent used the upstream lockfile with LiveKit Agents 1.8.2 and
the upstream LiveKit Inference models. Offline board discovery, fallback,
participant routing, RGB tools, console startup and voice-override checks
passed. Documentation, spelling, license and packaging checks also passed;
Doxygen retained existing warnings.

## Hardware results

| Test | Result |
| --- | --- |
| Korvo-1 voice_agent | Owner confirmed clear conversation, board identity, CPU temperature and green LED control |
| Korvo-1 replacement agent | New audio track subscribed and conversation resumed without resetting or flashing the board; owner confirmed |
| Function-CoreBoard-1 voice_agent | Owner confirmed the same requested voice/control checks; approximately 114 seconds before shutdown |
| Korvo-1 minimal_video with AEC | Original sustained receiver test passed: 628.216 seconds of video, 6,294 frames at 240 x 240, 10.017 fps average, maximum video gap 362 milliseconds, and 62,999 audio frames |

No firmware error or watchdog matches were found in these successful runs.
The first Korvo voice launch used the board's identity for the agent and
disconnected it; correcting the external launcher resolved this without
changing firmware or agent source. Use distinct participant identities.
Test agents were stopped, boards parked in the bootloader and owned
rooms verified empty after testing.

## Limits

A final pre-submission review on 2026-10-06 repeated the renderer and AEC
host suites, offline agent checks, lint/format, spelling and local component
packaging. Firmware and agent source remain unchanged from the hardware
candidate above. All three proposals merge cleanly into upstream `d492fa8`;
upstream has two subsequent Python lockfile updates. The S31 registry
workflow now pins an action with component manager 3.0.1, since the former
2.4.0 manager does not recognize S31 in its standalone upload environment.
At that time, the GitHub registry action had not been run on this proposal.

Those were combined-candidate smoke tests. They did not rerun the standalone
S3/P4 ESP-IDF 5.4.4/5.5.3 CI matrix or the registry dry-run action. Runtime
tests used pregenerated tokens; Sandbox HTTPS and subscriber-primary paths
remain unverified on this refresh. CoreBoard agent-restart recovery was not
separately tested.

The video result establishes continuous reception, not renewed visual or
listening acceptance. Cued speech playback and microphone-return tests were
not repeated. No heap/PSRAM trend was instrumented. Detailed overlap,
adaptive interruptions and self-triggering behavior of the updated agent
were not separately assessed. Earlier sustained AEC/listening results keep
their original configuration and scope; these results do not replace them.

## Compatibility correction validation on 2026-10-09

The candidate carries the shared dependency and ESP-IDF compatibility
corrections described in `dependency-validation.md` into the submitted S31
branch at `b685133`. This branch already declares its JSON and temperature
sensor dependencies explicitly, so their declarations need no further change.
The S31 board drivers, audio renderer, AEC integration and agent behavior
are unchanged by this correction.

All 18 S3/P4 application/version/target combinations passed locally with
IDF 5.4.4, 5.5.3 and 6.1 and in the separate [fork's six-job GitHub matrix](https://github.com/ibytergj/client-sdk-esp32/actions/runs/37927152386).
The S31 component registry dry-run also passed, including `s31_board`.

Seven fresh native S31 profiles passed with the documented IDF 6.1 toolchain
patch: `custom_hardware_s31`, `minimal` and `voice_agent` on both Korvo-1
and Function-CoreBoard-1, plus Korvo-1 `minimal_video`. Every image passed
its partition-size check. Both certificate settings remain enabled in all
seven generated configurations. Short local project paths were used to
avoid Windows path and command-line limits; application sources, defaults
and project CMake logic came from this candidate.

The existing renderer and AEC host suites passed ten repetitions each in
Docker, using capture 1.0.2 headers. The existing offline agent suite passed
against its locked runtime on Python 3.13: discovery, invalid-payload and
old-firmware fallback, linked-participant routing, RGB tools, real/console
startup and voice override. Agent lint and format checks also passed.

These results do not add new hardware or acoustic acceptance. No boards
were flashed. The P4 Hosted change and S3 bootloader requirements retain
the limits recorded in `dependency-validation.md`; the earlier attended
hardware results retain their original configuration and scope.
