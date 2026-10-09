/*
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import { writeFileSync } from 'node:fs';
import {
  AudioFrame,
  AudioSource,
  AudioStream,
  dispose,
  LocalAudioTrack,
  Room,
  RoomEvent,
  TrackKind,
  TrackPublishOptions,
  TrackSource,
} from '@livekit/rtc-node';

const TOKEN_ENDPOINT = 'https://cloud-api.livekit.io/api/sandbox/connection-details';
const PACKET_RELIABLE_TOPIC = 'lk.test.packet.reliable';
const PACKET_LOSSY_TOPIC = 'lk.test.packet.lossy';
const TEXT_TOPIC = 'lk.test.stream.text';
const BYTE_TOPIC = 'lk.test.stream.bytes';
const STATS_RPC_METHOD = 'lk.test.get_system_stats';
const WIFI_CYCLE_RPC_METHOD = 'lk.test.cycle_wifi';

function option(name, fallback) {
  const index = process.argv.indexOf(`--${name}`);
  return index >= 0 ? process.argv[index + 1] : fallback;
}

const roomName = option('room', 'esp32-interop');
const identity = option('identity', 'interop-test-peer');
const deviceIdentity = option('device', 'esp32-device');
const timeoutMs = Number(option('timeout-ms', '20000'));
const captureWav = option('capture-wav', undefined);
const testAudio = process.argv.includes('--audio');
const mediaOnly = process.argv.includes('--media-only');
const renderOnly = process.argv.includes('--render-only');
const captureOnly = process.argv.includes('--capture-only');
const fullDuplex = process.argv.includes('--full-duplex');
const disableDtx = process.argv.includes('--disable-dtx');
const audioBursts = Number(option('audio-bursts', testAudio ? '1' : '0'));
const audioIterations = Number(option('audio-iterations', '0'));
const wifiCycles = Number(option('wifi-cycles', '0'));
const wifiOutageMs = Number(option('wifi-outage-ms', '5000'));
const ledCycle = process.argv.includes('--led-cycle');
const toneHz = Number(option('tone-hz', '220'));
const toneLevel = Number(option('tone-level', '0.2'));
const toneChannel = option('tone-channel', 'mono').toLowerCase();
const toneGapMs = Number(option('tone-gap-ms', '0'));
const toneWarmupMs = Number(option('tone-warmup-ms', '2000'));
const toneDurationMs = Number(option('tone-duration-ms', '2000'));
const tonePostrollMs = Number(option('tone-postroll-ms', '2000'));
const captureFrames = Number(option('capture-frames', '25'));

if (!Number.isInteger(audioBursts) || audioBursts < 0) {
  throw new Error('--audio-bursts must be a non-negative integer.');
}
if (!Number.isInteger(audioIterations) || audioIterations < 0) {
  throw new Error('--audio-iterations must be a non-negative integer.');
}
if (audioBursts > 0 && audioIterations > 0) {
  throw new Error('Use --audio-bursts or --audio-iterations, not both.');
}
if (captureOnly && renderOnly) {
  throw new Error('Use --capture-only or --render-only, not both.');
}
if (mediaOnly && (ledCycle || wifiCycles > 0)) {
  throw new Error('--media-only cannot be combined with --led-cycle or --wifi-cycles.');
}
if (mediaOnly && !captureOnly && audioBursts === 0 && audioIterations === 0) {
  throw new Error('--media-only requires --audio, --audio-bursts, --audio-iterations, or --capture-only.');
}
if (fullDuplex && (renderOnly || captureOnly || audioBursts < 1 || audioIterations > 0)) {
  throw new Error(
    '--full-duplex requires --audio or --audio-bursts and cannot be combined with ' +
      '--capture-only, --render-only, or --audio-iterations.',
  );
}
if (!Number.isInteger(wifiCycles) || wifiCycles < 0) {
  throw new Error('--wifi-cycles must be a non-negative integer.');
}
if (!Number.isInteger(wifiOutageMs) || wifiOutageMs < 1000 || wifiOutageMs > 30000) {
  throw new Error('--wifi-outage-ms must be an integer from 1000 through 30000.');
}
if (!Number.isFinite(toneHz) || toneHz < 40 || toneHz > 2000) {
  throw new Error('--tone-hz must be a number from 40 through 2000.');
}
if (!Number.isFinite(toneLevel) || toneLevel <= 0 || toneLevel > 1) {
  throw new Error('--tone-level must be a number greater than 0 through 1.');
}
if (!['mono', 'left', 'right', 'both'].includes(toneChannel)) {
  throw new Error('--tone-channel must be mono, left, right, or both.');
}
if (!Number.isInteger(toneGapMs) || toneGapMs < 0 || toneGapMs > 5000) {
  throw new Error('--tone-gap-ms must be an integer from 0 through 5000.');
}
if (!Number.isInteger(toneWarmupMs) || toneWarmupMs < 0 || toneWarmupMs > 10000) {
  throw new Error('--tone-warmup-ms must be an integer from 0 through 10000.');
}
if (!Number.isInteger(toneDurationMs) || toneDurationMs < 100 || toneDurationMs > 30000) {
  throw new Error('--tone-duration-ms must be an integer from 100 through 30000.');
}
if (!Number.isInteger(tonePostrollMs) || tonePostrollMs < 0 || tonePostrollMs > 10000) {
  throw new Error('--tone-postroll-ms must be an integer from 0 through 10000.');
}
if (!Number.isInteger(captureFrames) || captureFrames < 1 || captureFrames > 3000) {
  throw new Error('--capture-frames must be an integer from 1 through 3000.');
}

function withTimeout(promise, label) {
  let timer;
  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(`Timed out waiting for ${label}`)), timeoutMs);
  });
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer));
}

function delay(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}

async function connectionDetails(tokenServerId) {
  const response = await fetch(TOKEN_ENDPOINT, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'X-Sandbox-ID': tokenServerId,
    },
    body: JSON.stringify({ roomName, participantName: identity }),
  });
  if (!response.ok) {
    throw new Error(
      `Development token server returned HTTP ${response.status}: ${await response.text()}`,
    );
  }
  return response.json();
}

function waitForParticipant(room) {
  for (const participant of room.remoteParticipants.values()) {
    if (participant.identity === deviceIdentity) return Promise.resolve(participant);
  }
  return withTimeout(
    new Promise((resolve) => {
      const listener = (participant) => {
        if (participant.identity === deviceIdentity) {
          room.off(RoomEvent.ParticipantConnected, listener);
          resolve(participant);
        }
      };
      room.on(RoomEvent.ParticipantConnected, listener);
    }),
    `participant '${deviceIdentity}'`,
  );
}

function waitForPacket(room, topic, expected) {
  return withTimeout(
    new Promise((resolve) => {
      const listener = (payload, participant, _kind, receivedTopic) => {
        if (
          receivedTopic === topic &&
          participant?.identity === deviceIdentity &&
          Buffer.from(payload).equals(expected)
        ) {
          room.off(RoomEvent.DataReceived, listener);
          resolve();
        }
      };
      room.on(RoomEvent.DataReceived, listener);
    }),
    topic,
  );
}

async function testPacket(room, reliable) {
  const topic = reliable ? PACKET_RELIABLE_TOPIC : PACKET_LOSSY_TOPIC;
  const payload = Buffer.from(reliable ? 'reliable-s31-packet' : 'lossy-s31-packet');
  const received = waitForPacket(room, `${topic}.echo`, payload);
  await room.localParticipant.publishData(payload, {
    reliable,
    topic,
    destination_identities: [deviceIdentity],
  });
  await received;
  console.log(`PASS ${reliable ? 'reliable' : 'lossy'} data packet echo`);
}

async function testTextStream(room) {
  const payload = 'S31 text stream: Wi-Fi 6 + RISC-V';
  const received = withTimeout(
    new Promise((resolve) => {
      room.registerTextStreamHandler(`${TEXT_TOPIC}.echo`, async (reader, participant) => {
        const text = await reader.readAll();
        if (participant.identity === deviceIdentity && text === payload) resolve();
      });
    }),
    'text stream echo',
  );
  await room.localParticipant.sendText(payload, {
    topic: TEXT_TOPIC,
    destinationIdentities: [deviceIdentity],
  });
  await received;
  console.log('PASS text stream echo');
}

async function testByteStream(room) {
  const payload = Uint8Array.from({ length: 128 }, (_, index) => index);
  const received = withTimeout(
    new Promise((resolve) => {
      room.registerByteStreamHandler(`${BYTE_TOPIC}.echo`, async (reader, participant) => {
        const chunks = await reader.readAll();
        const bytes = Buffer.concat(chunks.map((chunk) => Buffer.from(chunk)));
        if (participant.identity === deviceIdentity && bytes.equals(Buffer.from(payload))) resolve();
      });
    }),
    'byte stream echo',
  );
  const writer = await room.localParticipant.streamBytes({
    topic: BYTE_TOPIC,
    totalSize: payload.byteLength,
    destinationIdentities: [deviceIdentity],
  });
  await writer.write(payload);
  await writer.close();
  await received;
  console.log('PASS byte stream echo');
}

async function getSystemStats(room, responseTimeout = 5000) {
  const payload = await room.localParticipant.performRpc({
    destinationIdentity: deviceIdentity,
    method: STATS_RPC_METHOD,
    payload: '',
    responseTimeout,
  });
  const stats = JSON.parse(payload);
  for (const field of [
    'uptime_ms',
    'internal_free',
    'internal_min',
    'internal_largest',
    'psram_free',
    'psram_min',
    'psram_largest',
  ]) {
    if (!Number.isFinite(stats[field])) {
      throw new Error(`Invalid ${STATS_RPC_METHOD} field '${field}': ${payload}`);
    }
  }
  return stats;
}

async function testLedCycle(room) {
  for (const color of ['red', 'green', 'blue', 'white']) {
    await room.localParticipant.performRpc({
      destinationIdentity: deviceIdentity,
      method: 'set_led_state',
      payload: JSON.stringify({ color, state: true }),
      responseTimeout: 5000,
    });
    console.log(`PASS RGB LED ${color}`);
    await delay(1000);
  }
  await room.localParticipant.performRpc({
    destinationIdentity: deviceIdentity,
    method: 'set_led_state',
    payload: JSON.stringify({ color: 'white', state: false }),
    responseTimeout: 5000,
  });
  console.log('PASS RGB LED off');
}

async function testWifiRecovery(room, iteration, iterations) {
  const started = Date.now();
  const response = await room.localParticipant.performRpc({
    destinationIdentity: deviceIdentity,
    method: WIFI_CYCLE_RPC_METHOD,
    payload: String(wifiOutageMs),
    responseTimeout: 5000,
  });
  if (response !== 'scheduled') {
    throw new Error(`Unexpected ${WIFI_CYCLE_RPC_METHOD} response: '${response}'`);
  }

  // The firmware delays the stop briefly so the RPC response can leave the
  // device. Require at least one failed RPC before accepting recovery.
  await delay(1000);
  let outageObserved = false;
  let recoveredStats;
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      recoveredStats = await getSystemStats(room, 1500);
      if (outageObserved) break;
    } catch {
      outageObserved = true;
    }
    await delay(500);
  }
  if (!outageObserved) {
    throw new Error(`WiFi cycle ${iteration} did not produce an observable outage`);
  }
  if (!recoveredStats) {
    throw new Error(`WiFi cycle ${iteration} did not recover within ${timeoutMs} ms`);
  }

  await testPacket(room, true);
  console.log(
    `PASS WiFi/room recovery (${iteration}/${iterations}, ` +
      `${Date.now() - started} ms, uptime ${recoveredStats.uptime_ms} ms)`,
  );
}

function reportSystemStats(before, after) {
  const signed = (value) => `${value >= 0 ? '+' : ''}${value}`;
  console.log(
    `PASS system stats: internal free ${after.internal_free} B ` +
      `(${signed(after.internal_free - before.internal_free)} B), ` +
      `largest ${after.internal_largest} B; PSRAM free ${after.psram_free} B ` +
      `(${signed(after.psram_free - before.psram_free)} B), ` +
      `largest ${after.psram_largest} B; uptime ${after.uptime_ms} ms`,
  );
}

function waitForDeviceAudioTrack(room, participant) {
  for (const publication of participant.trackPublications.values()) {
    if (publication.kind === TrackKind.KIND_AUDIO && publication.track) {
      return Promise.resolve(publication.track);
    }
  }
  return withTimeout(
    new Promise((resolve) => {
      const listener = (track, publication, publisher) => {
        if (publisher.identity === deviceIdentity && publication.kind === TrackKind.KIND_AUDIO) {
          room.off(RoomEvent.TrackSubscribed, listener);
          resolve(track);
        }
      };
      room.on(RoomEvent.TrackSubscribed, listener);
    }),
    'ESP32 audio track',
  );
}

async function testIncomingAudio(room, participant) {
  const track = await waitForDeviceAudioTrack(room, participant);
  const stream = new AudioStream(track, { sampleRate: 16000, numChannels: 1 });
  const reader = stream.getReader();
  let frames = 0;
  let peak = 0;
  let samples = 0;
  let sumSquares = 0;
  const pcmChunks = [];
  try {
    await withTimeout(
      (async () => {
        while (frames < captureFrames) {
          const { done, value } = await reader.read();
          if (done) throw new Error(`ESP32 audio track ended after ${frames} frames`);
          frames++;
          if (captureWav) {
            const chunk = Buffer.alloc(value.data.length * 2);
            for (let i = 0; i < value.data.length; i++) chunk.writeInt16LE(value.data[i], i * 2);
            pcmChunks.push(chunk);
          }
          for (const sample of value.data) {
            peak = Math.max(peak, Math.abs(sample));
            sumSquares += sample * sample;
            samples++;
          }
        }
      })(),
      `${captureFrames} decoded ESP32 audio frames`,
    );
  } finally {
    await reader.cancel().catch(() => {});
  }
  const rms = samples > 0 ? Math.sqrt(sumSquares / samples) : 0;
  if (captureWav) {
    const pcm = Buffer.concat(pcmChunks);
    const header = Buffer.alloc(44);
    header.write('RIFF', 0); header.writeUInt32LE(36 + pcm.length, 4);
    header.write('WAVEfmt ', 8); header.writeUInt32LE(16, 16);
    header.writeUInt16LE(1, 20); header.writeUInt16LE(1, 22);
    header.writeUInt32LE(16000, 24); header.writeUInt32LE(32000, 28);
    header.writeUInt16LE(2, 32); header.writeUInt16LE(16, 34);
    header.write('data', 36); header.writeUInt32LE(pcm.length, 40);
    writeFileSync(captureWav, Buffer.concat([header, pcm]), { flag: 'wx' });
    console.log(`Saved microphone WAV: ${captureWav}`);
  }
  console.log(
    `PASS ESP32 Opus transport/decode (${frames} frames, ${samples} samples, ` +
      `PCM peak ${peak}, RMS ${rms.toFixed(2)})`,
  );
  if (peak < 32) {
    throw new Error(
      `Microphone PCM remained effectively silent (peak ${peak}, RMS ${rms.toFixed(2)})`,
    );
  }
}

function toneChannelCount() {
  return toneChannel === 'mono' ? 1 : 2;
}

async function captureTone(source, sampleRate, durationMs = toneDurationMs) {
  const channelCount = toneChannelCount();
  const samplesPerFrame = sampleRate / 100;
  const amplitude = Math.round(toneLevel * 32767);
  const frameCount = Math.ceil(durationMs / 10);
  for (let frameIndex = 0; frameIndex < frameCount; frameIndex++) {
    const frame = AudioFrame.create(sampleRate, channelCount, samplesPerFrame);
    for (let sampleIndex = 0; sampleIndex < samplesPerFrame; sampleIndex++) {
      const offset = frameIndex * samplesPerFrame + sampleIndex;
      const sample = Math.round(
        amplitude * Math.sin((2 * Math.PI * toneHz * offset) / sampleRate),
      );
      if (channelCount === 1) {
        frame.data[sampleIndex] = sample;
      } else {
        frame.data[sampleIndex * 2] = toneChannel === 'right' ? 0 : sample;
        frame.data[sampleIndex * 2 + 1] = toneChannel === 'left' ? 0 : sample;
      }
    }
    await source.captureFrame(frame);
  }
}

async function captureSilence(source, sampleRate, durationMs) {
  const channelCount = toneChannelCount();
  const samplesPerFrame = sampleRate / 100;
  const frameCount = Math.ceil(durationMs / 10);
  for (let frameIndex = 0; frameIndex < frameCount; frameIndex++) {
    const frame = AudioFrame.create(sampleRate, channelCount, samplesPerFrame);
    frame.data.fill(0);
    await source.captureFrame(frame);
  }
}

function toneDescription() {
  return `${toneHz} Hz ${toneChannel}`;
}

async function testOutgoingAudio(room, iteration, iterations) {
  const sampleRate = 48000;
  const source = new AudioSource(sampleRate, toneChannelCount());
  const track = LocalAudioTrack.createAudioTrack('s31-render-tone', source);
  const options = new TrackPublishOptions();
  if (disableDtx) options.dtx = false;
  options.source = TrackSource.SOURCE_MICROPHONE;
  const publication = await room.localParticipant.publishTrack(track, options);
  try {
    const subscriptionObserved = await Promise.race([
      publication.waitForSubscription().then(() => true),
      // LiveKit can forward a replacement track to the ESP32 before the
      // publisher-side first-subscription notification is delivered. Keep
      // driving the lifecycle stress case after a short grace period so it
      // exercises the device's SDP renegotiation instead of timing out here.
      delay(1500).then(() => false),
    ]);
    if (!subscriptionObserved) {
      console.log(
        `INFO publisher subscription signal was not observed for audio iteration ${iteration}; ` +
          'continuing the renegotiation stress test',
      );
    }
    await captureTone(source, sampleRate);
    await source.waitForPlayout();
    const suffix = iterations > 1 ? ` (${iteration}/${iterations})` : '';
    if (subscriptionObserved) {
      console.log(`PASS ESP32 subscribed to a ${toneDurationMs} ms ${toneDescription()} Opus render test${suffix}`);
    } else {
      console.log(`PASS published a ${toneDurationMs} ms ${toneDescription()} Opus renegotiation test${suffix}`);
    }
  } finally {
    if (publication.sid) await room.localParticipant.unpublishTrack(publication.sid);
    await track.close().catch(() => {});
    // Allow the server's inactive offer and the device decoder teardown to
    // complete before publishing a replacement track or measuring memory.
    await delay(500);
  }
}

async function testOutgoingAudioBursts(room, bursts) {
  const sampleRate = 48000;
  const source = new AudioSource(sampleRate, toneChannelCount());
  const track = LocalAudioTrack.createAudioTrack('s31-render-soak', source);
  const options = new TrackPublishOptions();
  if (disableDtx) options.dtx = false;
  options.source = TrackSource.SOURCE_MICROPHONE;
  const publication = await room.localParticipant.publishTrack(track, options);
  try {
    await withTimeout(publication.waitForSubscription(), 'ESP32 audio subscription');
    if (toneWarmupMs > 0) {
      await captureSilence(source, sampleRate, toneWarmupMs);
      console.log(`SENT ${toneWarmupMs} ms silent renderer warm-up`);
    }
    for (let burst = 1; burst <= bursts; burst++) {
      await captureTone(source, sampleRate);
      console.log(
        `SENT ${toneDurationMs} ms ${toneDescription()} Opus burst (${burst}/${bursts})`,
      );
      if (burst < bursts && toneGapMs > 0) {
        await captureSilence(source, sampleRate, toneGapMs);
      }
    }
    if (tonePostrollMs > 0) {
      await captureSilence(source, sampleRate, tonePostrollMs);
      console.log(`SENT ${tonePostrollMs} ms silent renderer post-roll`);
    }
    await source.waitForPlayout();
    console.log('PASS host audio-source playout queue drained before unpublish');
  } finally {
    if (publication.sid) await room.localParticipant.unpublishTrack(publication.sid);
    await track.close().catch(() => {});
    await delay(500);
  }
}

async function main() {
  const tokenServerId = option('token-server-id', process.env.LIVEKIT_TOKEN_SERVER_ID);
  if (!tokenServerId) {
    throw new Error('Set LIVEKIT_TOKEN_SERVER_ID or pass --token-server-id.');
  }

  const details = await connectionDetails(tokenServerId);
  const room = new Room();
  try {
    await room.connect(details.serverUrl, details.participantToken, {
      autoSubscribe: !renderOnly,
    });
    console.log(`Connected '${identity}' to room '${details.roomName}'`);
    const device = await waitForParticipant(room);
    console.log(`Found ESP32 participant '${deviceIdentity}'`);
    let statsBefore;
    if (mediaOnly) {
      console.log('SKIP test-firmware RPC, data, stream, LED, and memory checks (--media-only)');
    } else {
      statsBefore = await getSystemStats(room);

      const temperature = await room.localParticipant.performRpc({
        destinationIdentity: deviceIdentity,
        method: 'get_cpu_temp',
        payload: '',
        responseTimeout: 5000,
      });
      if (!Number.isFinite(Number(temperature))) {
        throw new Error(`Unexpected CPU temperature RPC response: '${temperature}'`);
      }
      console.log(`PASS RPC get_cpu_temp (${temperature} C)`);

      await testPacket(room, true);
      await testPacket(room, false);
      await testTextStream(room);
      await testByteStream(room);
      if (ledCycle) await testLedCycle(room);
      for (let iteration = 1; iteration <= wifiCycles; iteration++) {
        await testWifiRecovery(room, iteration, wifiCycles);
      }
    }
    if (captureOnly || audioBursts > 0 || audioIterations > 0) {
      const currentDevice = await waitForParticipant(room);
      if (fullDuplex) {
        await Promise.all([
          testIncomingAudio(room, currentDevice),
          testOutgoingAudioBursts(room, audioBursts),
        ]);
      } else if (!renderOnly) {
        await testIncomingAudio(room, currentDevice);
      } else {
        console.log('SKIP microphone subscription (--render-only)');
      }
      if (audioBursts > 0 && !fullDuplex) {
        await testOutgoingAudioBursts(room, audioBursts);
      }
      for (let iteration = 1; iteration <= audioIterations; iteration++) {
        await testOutgoingAudio(room, iteration, audioIterations);
      }
    }
    if (!mediaOnly) {
      const statsAfter = await getSystemStats(room);
      reportSystemStats(statsBefore, statsAfter);
    }
    console.log(
      mediaOnly ? 'All requested ESP32 media transport checks passed.' : 'All requested ESP32 interoperability checks passed.',
    );
  } finally {
    await room.disconnect();
    await dispose();
  }
}

main().catch((error) => {
  console.error(`FAIL ${error.stack ?? error}`);
  process.exitCode = 1;
});
