# Fire-chan Project Progress

**Target:** M5Stack Fire v2.5  
**Working directory:** `D:\projects\Fire-Chan`  
**Diagnostic serial port:** `COM5` at 115200 baud  
**Last updated:** 2026-08-04

## Current milestone

Phase 0 (repository and toolchain) is complete. Phase 1 (hardware diagnostics) is substantially complete with PSRAM investigation and manual checks outstanding. Phase 2 (face engine) is implemented and approved on hardware. Expression-driven personality feedback is now running as firmware version `0.3.0-personality`.

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
| Firmware | `0.3.0-personality` |
| Static RAM use | 26,016 bytes (0.6%) |
| Flash use | 524,181 bytes (8.0%) |
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

1. Resolve or formally document the PSRAM limitation.
2. Complete the remaining manual diagnostic confirmations.
3. Record the exact IMU and power-controller models.
4. Split the diagnostic firmware into hardware abstraction and diagnostic modules.
5. Add the first non-blocking animated face and button-driven expressions.

## Useful commands

```powershell
pio run
pio run --target upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```
