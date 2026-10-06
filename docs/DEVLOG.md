# Radar BLE Bridge - Development Log

Running log of problems and fixes per phase. Source material for the
README "Challenges" section and the resume bullets.

Format for each entry:

- **Symptom:** what was observed (nRF Connect, USB log, behaviour)
- **Cause:** what was actually wrong
- **Fix:** what changed, and how it was verified

## Phase 1 - BTstack bring-up

Goal: advertise a custom 128-bit service, accept a connection from
nRF Connect, notify a heartbeat counter.

### Issues

<!-- add entries here -->

## Phase 2 - Radar data over GATT

### Issues

## Phase 3 - Robustness and performance

### Measurements

Phones: an iOS device (nRF Connect for iOS), Xiaomi Pad (nRF Connect for Android).

| Item | Setting | Result |
| --- | --- | --- |
| ATT MTU, iOS | negotiated automatically | 251 (bridge limit; iOS offers up to 527) |
| ATT MTU, Android | default, no request | 23 |
| ATT MTU, Android | Request MTU 517 | 251 |
| Connection interval, Android | on connect | 45 ms, latency 0, supervision timeout 5000 ms |
| Connection interval, Android | during service discovery | briefly 7.5 ms, then back to 45 ms |
| Connection interval, Android | connection priority HIGH | 15 ms |
| Connection interval, Android | bridge requests 15-30 ms | 22.5 ms, supervision timeout 4000 ms |
| Connection interval, Android | bridge requests 7.5-15 ms | 15 ms (7.5 ms not granted) |
| Connection interval, Android | bridge requests 100-125 ms | 112.5 ms |
| LE Data Length | on connect / after MTU 251 | tx 251, rx 27 / tx 251, rx 251 |
| UART frame rate | radar sweeping | ~17-20 frames/s |
| UART integrity | 18,118 frames | 0 bad, 0 dropped |
| Throughput, test mode | MTU 23, interval 22.5 ms | 155.7 kbit/s (10,299 packets in 10 s, ~23 packets per connection event) |
| Throughput, test mode | MTU 251, interval 22.5 ms | 466.0 kbit/s (2,360 packets in 10 s, ~5.3 per connection event) |
| Throughput, test mode | MTU 251, interval 15 ms | 402.6 kbit/s (2,042 packets in 10 s, ~3.1 per connection event) |
| Throughput, test mode | MTU 251, interval 112.5 ms | 239.6 kbit/s (1,214 packets in 10 s, ~13.7 per connection event) |
| Latency UART -> notify | 100 ms window, 50 ms tick, MTU 23 | avg 20-35 ms, max ~150 ms |
| Latency UART -> notify | 100 ms window, 50 ms tick, MTU 251 | avg 115-130 ms, max ~150 ms |
| Latency UART -> notify | 20 ms window, exact timer, MTU 251 | (to fill in) |

### Robustness tests

| Test | Result |
| --- | --- |
| Phone disconnects | `Disconnected`, advertising resumes, reconnect works |
| Phone Bluetooth switched off | Disconnect detected after the supervision timeout, advertising resumes |
| UART wire pulled | `Radar link OFFLINE` within ~1 s |
| UART wire restored | `Radar link online`, `$Q` sent, thresholds re-read |
| Nucleo reset | Link offline briefly; thresholds revert to 400/150 (kept in RAM only) |
| Pico reset | Phone disconnects; bridge re-advertises and reconnects |

### Issues

- **Symptom:** "Connection parameters" printed twice on every connect.
  **Cause:** one packet handler was registered both as the HCI event
  handler and as the ATT server handler, so the LE connection-complete
  event reached it twice.
  **Fix:** separate HCI and ATT handlers.
- **Symptom:** Android stays at MTU 23 until the app asks for more.
  **Cause:** only the GATT client can start an MTU exchange; iOS does it
  automatically, Android does not.
  **Fix:** none needed on the bridge; batching already sizes packets
  from the current MTU (4 samples at 23, 61 at 251).
- **Symptom:** latency got worse with a bigger MTU (avg ~120 ms at 251
  vs ~30 ms at 23), and the connection interval made no difference.
  **Cause:** at ~18 samples/s a 61-sample packet never fills, so every
  sample waited for the 100 ms batch timeout, checked only on a 50 ms
  tick (100-150 ms). At MTU 23 the Nucleo's bursts of frames filled the
  4-sample packets quickly. The radar uses under 1% of the link, so the
  long batch window bought nothing.
  **Fix:** 20 ms window with an exact one-shot timer armed when a sample
  enters an empty queue.
- **Observation:** MTU 251 with DLE gives 3x the throughput of MTU 23
  (466 vs 156 kbit/s at 22.5 ms). A shorter interval did not help:
  15 ms gave less than 22.5 ms, at about 200-240 packets/s in both cases.
  Likely limit: the host-to-controller path (only 3 controller ACL
  buffers, refilled over the cyw43 shared bus), not the air interface.
  At 112.5 ms the per-event packet count caps throughput instead.
  Not yet proven; testing would mean raising the buffer count, which
  the Pico SDK keeps low to avoid cyw43 bus overruns.
- **Known limitation:** thresholds written over BLE are lost when the
  Nucleo resets (stored in RAM). Possible fix: persist them in flash.

## Phase 4 - Security

### Issues

## Resume / README notes

<!-- one line per thing worth mentioning, with numbers where possible -->
