# Fire-chan

[License](LICENSE)  ·  v0.12.0-alarm-ringing (firmware) / 0.7.2 (gateway)

A stationary, Stack-chan-inspired desktop companion on the M5Stack Fire v2.5
with a privacy-first local voice assistant named **Ember**. No camera, no servos.

---

## Status

> [!CAUTION]
>
> ### 🛑 WORK IS PAUSED — PROJECT ON HOLD
>
> Development has stopped. All milestones through **gateway 0.8.0** /
> **firmware 0.12.0-alarm-ringing** are preserved and committed on both
> `main` and `oc-updates`. The project is parked pending next hardware:
> **ESP32-P4** (→ `Fire-chan_ESP32-P4_Variant.md`).

---

## What's working

- Animated face (14 expressions, blink, gaze, mouth)
- Buttons and IMU gestures via an event-driven behavior engine
- Push-to-talk voice capture at 16 kHz / 16-bit mono
- Local voice stack on a Raspberry Pi 5: whisper.cpp `base.en`, Ollama
  `llama3.2:3b`, Piper `en_GB-alba-medium`; optional Gemini with Ollama fallback
- Deterministic local commands: time, date, status, help, repeat, volume,
  sleep/wake, mute/unmute, timers, alarms, weather, web search, calculations,
  unit conversions
- Timers and alarms on the Fire:
  - **Timers** are announced on the next push-to-talk turn when due
  - **Alarms** fire locally: a 3-second buzzer at the set time with the
    Alarmed expression; dismissed with Button B (or hold A to talk over it)
- NTP-synced clock for absolute alarm times (survives reboot via NVS)
- 8 kHz canonical WAV replies on the Pi to cut response bytes ~3.5x
- Accepted speech output level 141
- Persistent NVS config with microSD backup
- Gateway auth, history per device, no-speech handled gracefully (no error face)

## Known limitations / outstanding goals

- **Streaming early-start playback** — 0.12.0-stream-play / 0.12.0-stream-3buf
  both rolled back (red error, then lockup). Known-good: download-then-play.
- **Alarm snooze and missed-reminder behavior** — ringing + dismiss done; snooze
  and missed handling not yet implemented.
- **Device-friendly Wi-Fi provisioning** — the device is provisioned via
  `include/secrets.h` (gitignored). The captive-portal flow is the next big
  priority.
- **PSRAM** — boot-time test fails on this Fire v2.5; not required because all
  buffers are bounded internal RAM.
- **Stability / overnight runs** — short runs are clean, but multi-day tests
  and voice cancellation are not yet verified.

## Build and upload

```bash
# (PowerShell on Windows, replace COMxx with your port)
pio run
pio run --target upload --upload-port COMxx
pio device monitor --port COMxx --baud 115200
```

Copy `include/secrets.example.h` to `include/secrets.h` and fill in the
local network and gateway token. The real secrets file is gitignored and must
never be committed.

## Documentation

- `Fire-chan_Project_Brief_v0.1.md` — goal and scope
- `Fire-chan_Project_Design_Specification_v0.1.md` — design
- `future-upgrade.md` — next hardware platform (ESP32-P4)
- `TODO.md` — prioritized development roadmap
- `THIRD_PARTY_NOTICES.md` — third-party licenses
- `LICENSE` — Apache 2.0

The detailed change log, slice notes, and deployment instructions live in
`opencode_update.md`, `PROJECT_PROGRESS.md`, and `gateway/README.md`.

## License

Apache 2.0. See `THIRD_PARTY_NOTICES.md` for third-party library, model,
and voice-asset licenses before redistributing.