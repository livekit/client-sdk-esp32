# Generic transport fixes: validation scope

The proposal is based on SDK v0.3.11 and addresses remote audio selection,
subscriber data-channel configuration and HTTPS Sandbox token acquisition.

## Validation on 2026-09-20

With the proposed dependency and S31-support changes, the Korvo voice
example recovered after its remote agent was restarted without resetting
or flashing the board. Device logs showed the previous audio participant
disconnecting and subscription to the replacement audio track. The owner
confirmed that the agent spoke and answered again.

The combined candidate also passed voice/board-control checks on both S31
boards and the existing sustained Korvo audio/video receiver test. Seven
S31 example profiles built on ESP-IDF 6.1 and fit their partitions.

The runtime tests used pregenerated room tokens, so they do not establish
Sandbox HTTPS behavior. Subscriber-primary behavior and failure-path cleanup
remain runtime coverage gaps. The standalone S3/P4 CI matrix was not rerun.
Successful combined-candidate tests do not independently isolate each fix.

The data-channel change aligns configuration with negotiated peer roles.
esp_peer 1.5.5 already guards against creating channels when the SDP lacks
an application section; no reproduction of the older indefinite retry
symptom on that version is claimed.
