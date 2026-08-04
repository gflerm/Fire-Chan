# Fire-chan

Fire-chan is a stationary, Stack-chan-inspired desktop companion built for the
M5Stack Fire v2.5. The project combines an expressive animated face, buttons,
IMU gestures, RGB lighting, local audio, and a privacy-friendly voice assistant
named Ember—without a camera or servo movement.

## Current status

Firmware `0.9.0-assistant-directives` is running on the physical device.

- Animated face with 14 expressions, blinking, gaze, and speaking animation
- Button and IMU interactions through a modular event-driven behavior engine
- Persistent configuration in NVS with microSD backup
- Push-to-talk 16 kHz voice recording
- Fully local STT, conversation, and TTS on a Raspberry Pi 5
- Streamed response playback without requiring PSRAM
- Ember-driven expressions and sleep, wake, mute, and unmute actions
- Accepted speech output level of 141

The M5Stack's PSRAM currently fails its boot-time hardware test, but PSRAM is
optional: the implemented face, voice, networking, and playback features use
bounded internal-RAM buffers and remain operational.

## Local voice stack

The Raspberry Pi gateway keeps voice processing on the local network:

- whisper.cpp `base.en` for speech recognition
- Ollama `llama3.2:3b` for conversation
- Piper `en_GB-alba-medium` for Ember's voice
- FastAPI gateway for authentication, local commands, and response audio

No cloud AI account is required for the current voice path.

## Build and upload

The project uses PlatformIO with the Arduino framework:

```powershell
pio run
pio run --target upload --upload-port COM5
pio device monitor --port COM5 --baud 115200
```

Copy `include/secrets.example.h` to `include/secrets.h` and enter local network
and gateway credentials. The real secrets file is ignored by Git and must never
be committed.

## Documentation

- `Fire-chan_Project_Brief_v0.1.md` — objective and scope
- `Fire-chan_Project_Design_Specification_v0.1.md` — detailed design
- `PROJECT_PROGRESS.md` — verified implementation and hardware results
- `TODO.md` — prioritized development roadmap
- `DECISIONS.md` — architecture decisions
- `gateway/README.md` — Raspberry Pi installation and operation

The next milestone focuses on device-friendly Wi-Fi provisioning, multi-turn
conversation, voice interruption, and extended stability testing.
