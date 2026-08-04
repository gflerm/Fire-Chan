# Fire-chan Project Progress

**Target:** M5Stack Fire v2.5  
**Working directory:** `D:\projects\Fire-Chan`  
**Diagnostic serial port:** `COM5` at 115200 baud  
**Last updated:** 2026-08-04

## Current milestone

Phases 0–3 are substantially complete: the repository/toolchain, hardware baseline, face engine, modular inputs, event bus, and behavior engine all run on the physical device. Phase 4 local audio has non-blocking expression cues, volume, and persistent mute; SD sound packs and alarm playback remain. The storage/configuration foundation is complete, and the first Phase 8 push-to-talk input milestone is running as firmware version `0.6.0-voice-input`. Networking and cloud-backed transcription remain next.

## Completed

- [x] Created the local Git repository on branch `main`.
- [x] Added and committed the project brief and design specification.
- [x] Created a PlatformIO project for `m5stack-fire` using the Arduino framework.
- [x] Added M5Unified and Adafruit NeoPixel dependencies.
- [x] Configured upload and monitoring on `COM5` at 115200 baud.
- [x] Built the firmware successfully with PlatformIO.
- [x] Uploaded diagnostic firmware to the connected M5Stack Fire v2.5.
- [x] Captured serial diagnostics from the physical device.
- [x] Refactored the firmware into application, face, animation, input, and gesture modules.
- [x] Added an internal-RAM 8-bit display canvas that does not require PSRAM.
- [x] Implemented natural non-blocking blinking, idle gaze, tilt-controlled pupils, and mouth animation.
- [x] Implemented every specified expression preset.
- [x] Added automatic expression showcase and manual button controls.
- [x] Added filtered pickup, shake, face-down, face-up, and inactivity gesture handling.
- [x] Built, uploaded, and exercised firmware version `0.2.0-face` on the physical device.
- [x] Added blink-covered expression transitions and background color blending.
- [x] Added non-blocking expression sound cues in a dedicated audio module.
- [x] Added animated expression-specific lighting for all ten RGB LEDs.
- [x] Added long-C sound mute/unmute control.
- [x] Built, uploaded, and exercised firmware version `0.3.0-personality`.
- [x] Added a fixed-capacity event bus without dynamic allocation.
- [x] Routed buttons, IMU gestures, inactivity, and the demo scheduler through typed events.
- [x] Added a central behavior engine with specification-aligned state priority.
- [x] Added behavior inputs for future alarm, error, listening, speaking, and network states.
- [x] Removed direct input-to-face coupling from the application orchestrator.
- [x] Built, uploaded, and exercised firmware version `0.4.0-events`.
- [x] Added an architecture decision log covering PlatformIO, optional PSRAM, and storage responsibilities.
- [x] Added validated configuration defaults for display, audio, RGB, gestures, inactivity, and demo mode.
- [x] Added NVS persistence for essential preferences with deferred, non-blocking writes.
- [x] Added a human-readable `/config/device.json` backup on microSD.
- [x] Added safe fallback behavior when NVS or microSD is unavailable.
- [x] Applied configuration to the face, audio, RGB, gesture, inactivity, and behavior modules.
- [x] Built, uploaded, and exercised firmware version `0.5.0-config`.
- [x] Remapped Button A to specification-aligned hold-to-talk voice capture.
- [x] Added a modular, bounded, non-PSRAM voice recorder.
- [x] Streamed 16 kHz, 16-bit mono WAV prompts to `/recordings/last_prompt.wav` on microSD.
- [x] Added Listening and Thinking behavior transitions around voice capture.
- [x] Suspended the speaker during recording to avoid microphone feedback.
- [x] Replaced the slow M5Unified analog-I2S path with an exact-rate GPIO34 timer sampler.
- [x] Captured a 3.546-second physical voice test as a 113,500-byte WAV without buffer overrun.
- [x] Built, uploaded, and exercised firmware version `0.6.0-voice-input`.

## Voice input milestone

| Item | Result |
|---|---|
| Control | Hold Button A to record; release to stop |
| Privacy feedback | Listening face and RGB state while recording |
| Post-capture feedback | Thinking face |
| Recording format | 16 kHz, signed 16-bit, mono WAV |
| Recording path | `/recordings/last_prompt.wav` |
| Maximum duration | 12 seconds, configurable and validated |
| Buffering | Two 4096-sample internal-RAM buffers |
| PSRAM dependency | None |
| Physical duration test | 3.546 seconds |
| Recorded payload | 113,500 bytes |
| Buffer overrun | None observed |
| Face rate while recording | approximately 17 FPS |
| Remaining voice path | STT, local command routing, optional AI, TTS playback |

## Storage and configuration milestone

| Item | Result |
|---|---|
| NVS settings load | PASS |
| Safe compiled defaults | Implemented and validated before use |
| microSD mount | PASS |
| `/config/device.json` backup | PASS |
| Missing-card handling | Implemented; local features continue without SD |
| Deferred preference writes | Implemented with a 1.5-second debounce |
| Configured subsystems | Display, sound, RGB, gestures, inactivity, demo mode |
| Runtime frame rate | 27.7–27.8 FPS |
| Runtime free heap | approximately 207 KB and stable |
| PSRAM dependency | None |

Persistent button-controlled preferences:

- Hold Button C: mute/unmute sound
- Button C: automatic/manual showcase mode

## Face milestone

Implemented expression presets:

- Neutral
- Happy
- Excited
- Sad
- Surprised
- Confused
- Sleepy
- Sleeping
- Listening
- Thinking
- Speaking
- Alarmed
- Offline
- Error

Runtime test results:

| Item | Result |
|---|---|
| All 14 expressions scheduled | PASS |
| Non-blocking rendering | PASS |
| Average frame rate | 28.6 FPS |
| Free heap after canvas allocation | approximately 248 KB |
| Heap stability during showcase | PASS; stable across the captured cycle |
| PSRAM dependency | None; canvas forced to internal RAM |
| Automatic blink and idle gaze | Running |
| Speaking mouth animation | Running |
| Physical appearance approval | Pending user observation |
| Buttons and physical IMU gestures | Pending; no events were received during the timed manual capture |

Personality feedback test results:

| Item | Result |
|---|---|
| Expression transitions | PASS; blink-covered transition remains non-blocking |
| RGB feedback scheduler | PASS; initialized and ran alongside the face |
| Audio cue scheduler | PASS; initialized and queued cues without blocking |
| Combined frame rate | 27.8 FPS |
| Combined free heap | approximately 237 KB and stable |
| Pickup gesture | PASS; physical event captured |
| Long-C mute | PASS; physical button event captured and mute state changed |
| Remaining buttons/gestures | Manual confirmation still required |

Event and behavior test results:

| Item | Result |
|---|---|
| Event queue | PASS; fixed capacity of 16 events |
| Automatic demo routing | PASS; each change dispatched as an event |
| Behavior resolution | PASS; every demo event resolved to the expected expression |
| Queue drain | PASS; pending count returned to zero after each dispatch |
| Dropped events | None observed |
| Priority order | Error > alarm > face-down > speaking > listening > temporary reaction > offline > base state |
| Event-driven frame rate | 27.8 FPS |
| Event-driven free heap | approximately 236.6 KB and stable |

Event implementation modules:

- `src/app/AppEvent.*`
- `src/app/EventBus.*`
- `src/app/BehaviorEngine.*`

Test controls:

| Input | Test action |
|---|---|
| Button A | Previous expression and pause automatic showcase |
| Button B | Next expression and pause automatic showcase |
| Button C | Toggle automatic/manual showcase |
| Hold Button B | Return to Neutral |
| Hold Button C | Mute or unmute expression sounds |
| Tilt | Move pupils |
| Pick up | Temporary Surprised reaction |
| Shake | Temporary Confused reaction |
| Face down | Sleeping expression |
| Face up | Wake to Neutral |
| 30 seconds inactive | Sleepy expression in manual mode |
| 60 seconds inactive | Sleeping expression in manual mode |

Face implementation modules:

- `src/app/Application.*`
- `src/face/Expression.*`
- `src/face/AnimationScheduler.*`
- `src/face/EyeRenderer.*`
- `src/face/MouthRenderer.*`
- `src/face/FaceEngine.*`
- `src/input/GestureDetector.*`
- `src/input/InputManager.*`
- `src/audio/AudioFeedback.*`
- `src/hardware/RgbFeedback.*`

## Build baseline

| Item | Result |
|---|---|
| PlatformIO environment | `firechan` |
| PlatformIO board | `m5stack-fire` |
| ESP32 platform | Espressif 32 6.13.0 |
| Arduino ESP32 framework | 3.20017.241212 |
| M5Unified | 0.2.19 |
| Adafruit NeoPixel | 1.15.5 |
| Firmware | `0.6.0-voice-input` |
| Static RAM use | 42,884 bytes (0.9%) |
| Flash use | 590,425 bytes (9.0%) |
| Build | PASS |
| Upload | PASS |

## Detected hardware

| Item | Value |
|---|---|
| USB serial adapter | CH9102 |
| ESP32 | ESP32-D0WDQ6-V3, revision 3.1 |
| CPU cores | 2 |
| Flash | 16 MB |
| Display | 320 x 240 |
| microSD | 15,193 MB detected |

## Diagnostic results

Results from the final diagnostic boot on 2026-08-04:

| Subsystem | Result | Evidence / notes |
|---|---|---|
| Firmware build | PASS | Clean PlatformIO build |
| Firmware upload | PASS | Image written and verified on `COM5` |
| Display | PASS | M5Unified initialized a 320 x 240 display and rendered the status screen |
| Internal heap | PASS | 274,992 bytes free at startup; about 230 KB during live reporting |
| PSRAM | **FAIL** | Boot memory test failed repeatedly; `ESP.getPsramSize()` returned 0 |
| Power controller | PARTIAL | Battery level reported 100%, but voltage reported 0 mV |
| IMU | PASS | Acceleration near stationary gravity; live readings remained stable |
| RGB LEDs | CHECK | Red, green, and blue sequence was transmitted; visual confirmation is still required |
| Speaker | CHECK | Two tones were transmitted; audible confirmation is still required |
| Microphone | PASS | RMS 6580.0, sample range -32752 to 32751 |
| microSD | PASS | 15,193 MB card mounted successfully |
| Wi-Fi radio | PASS | Scan completed and found 6 networks on the final run |
| Buttons A/B/C | CHECK | Live event reporting is active; each physical button still needs to be pressed and observed |
| Runtime stability | PASS (short run) | Device stayed responsive through startup and live telemetry capture |

## Active findings

### PSRAM failure

The official PlatformIO `m5stack-fire` board profile enables PSRAM, but the ESP32 boot-time SRAM test failed on repeated boots. The final boot reported 104,426 failed writes out of 131,072 and no usable PSRAM.

This is not an application scheduling or startup-delay issue: the test fails in the ESP32 runtime before `setup()` begins. The project already uses M5Stack's documented `m5stack-fire` board selection and PSRAM build flag. A marginal SPI bus clock or electrical issue is possible, but the leading causes are a GPIO16/GPIO17 conflict from attached hardware, unstable power, an incompatible PSRAM configuration for the exact board revision, or faulty PSRAM hardware.

Next checks:

1. Remove any stacked module or external connection that may drive GPIO16 or GPIO17, then cold-boot and retest.
2. Test with the official M5Stack factory or PSRAM diagnostic firmware.
3. Test the M5Stack-documented Espressif 32 platform version 6.12.0 to rule out a toolchain regression.
4. Check power stability and USB cable quality.
5. If the factory test also fails with no expansion hardware attached, treat this as a probable hardware fault.
6. Keep early firmware operational without PSRAM until the cause is resolved.

### Power reporting

Battery percentage is available, but battery voltage is currently reported as 0 mV. Confirm the power-controller variant and its supported M5Unified readings before relying on battery telemetry.

## Manual checks to complete

- [ ] Confirm the LCD text and colors are visually correct.
- [ ] Confirm all ten RGB LEDs show red, green, and blue.
- [ ] Confirm both speaker tones are audible and undistorted.
- [ ] Press buttons A, B, and C while monitoring serial output.
- [ ] Tilt and move the unit while confirming changing live IMU readings.
- [ ] Repeat the microphone test while speaking near the microphone.
- [ ] Run a longer stability and memory-leak test.

## Next development steps

1. Apply local action and expression hints returned by the gateway.
2. Add captive Wi-Fi provisioning and move the compiled local credentials into NVS.
3. Tune Ember's voice volume, pace, and personality after listening tests.
4. Complete Phase 4 with microSD sound-pack playback and reusable alarm audio.
5. Run longer multi-turn stability, reconnection, and audio-interruption tests.
6. Continue the separate PSRAM hardware/configuration investigation without blocking feature work.

## Raspberry Pi voice gateway

Prepared on 2026-08-04 for a Raspberry Pi 5 with 8 GB RAM and an SSD:

| Component | Selection | Purpose |
|---|---|---|
| Speech recognition | whisper.cpp `base.en` | Local English speech-to-text |
| Conversation | Ollama `llama3.2:3b` | Local short-form replies |
| Voice | Piper `en_GB-alba-medium` | Ember's warm British English voice |
| Orchestration | Ember FastAPI gateway | Authentication, commands, model routing, and response audio |

The versioned `gateway/` package includes the application, local command router,
personality prompt, automatic installer, and system services. The gateway uses one
LAN-facing authenticated endpoint; model services remain bound to localhost. Unit
tests for command routing pass. Deployment and live end-to-end tests on the Pi 5 SSD pass.

### Fire-to-Pi connection

Firmware `0.7.0-pi-gateway` adds a modular Wi-Fi manager and an assistant client for
the gateway at `192.168.8.107:8088`. Long HTTP requests and SD reads run in a dedicated
FreeRTOS task so the face loop stays responsive. Credentials live only in the ignored
`include/secrets.h`; `include/secrets.example.h` documents the required values.

The firmware builds successfully with no warnings. Live hardware verification passed on
2026-08-04: the Fire joined Wi-Fi as `192.168.8.124` at -44 dBm, uploaded a 3.891-second
124,528-byte recording, and stayed responsive at roughly 25–27 FPS during processing.
The Pi transcribed "What is your name?" exactly and returned "I'm Ember. It's lovely to
meet you." The response-ready expression was applied successfully.

Firmware `0.8.0-ember-voice` completes the spoken round trip without PSRAM. It accepts
fixed-length or connection-close HTTP audio, atomically caches each reply on microSD,
validates the WAV structure, and streams 16-bit PCM through two alternating 1,536-byte
internal-RAM buffers. Two live responses (113,196 and 62,508 bytes, 22.05 kHz mono)
played to completion while the Speaking expression was active. The face returned to
roughly 27 FPS and free heap recovered after each reply. Audible clarity and preferred
volume remain a user-listening check.

Initial listening feedback described the reply as soft and distorted. The player was
updated to three rotating buffers (the M5Unified safe runtime-audio pattern), restores an
explicit speech volume of 104 after speaker restart, and reports source peak level. Two
further responses completed with stable memory and a source peak of 32,767. Subjective
clarity after this adjustment remains to be confirmed; the peak result also gives a basis
for adding attenuation or Pi-side audio conditioning if needed.

The saved expression demo had also been competing with assistant states. Beginning a real
voice capture now turns demo mode off and persists the change. Live testing confirmed the
face remains stable between prompts and uses Listening, Thinking, and Speaking only for
the corresponding interaction stages.

Firmware `0.8.1-voice-tuning` raises the dedicated speech output by 10% (level 104 to
114 at the current 65% user setting) without changing expression-cue volume. Gateway
`0.2.0` handles time questions deterministically from `Africa/Johannesburg` instead of
allowing the language model to guess. The Pi was independently verified at 18:47 SAST,
with NTP active and the clock synchronized; the earlier 15:47 answer was therefore a
model hallucination, not a Pi clock fault. Five gateway command tests pass.

After confirming the time response was correct, listening feedback requested slightly
more output. Firmware `0.8.2-voice-tuning` raises speech from level 114 to 128 (about
12% additional gain at the current setting); expression tones remain unchanged.

Firmware `0.8.3-voice-tuning` applies the next listening adjustment, raising speech
from level 128 to 141 (about 10%) while leaving expression tones unchanged.

## Useful commands

```powershell
pio run
pio run --target upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```
