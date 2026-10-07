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
The GitHub registry action has not been run on this proposal.

These are combined-candidate smoke tests. They do not rerun the standalone
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
