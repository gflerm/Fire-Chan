# Fire-chan

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)

Fire-chan is a stationary, Stack-chan-inspired desktop companion built for the
M5Stack Fire v2.5. The project combines an expressive animated face, buttons,
IMU gestures, RGB lighting, local audio, and a privacy-friendly voice assistant
named Ember—without a camera or servo movement.

## Current status

Firmware `0.9.3-audio-download` is running on the physical device.

- Animated face with 14 expressions, blinking, gaze, and speaking animation
- Button and IMU interactions through a modular event-driven behavior engine
- Persistent configuration in NVS with microSD backup
- Push-to-talk 16 kHz voice recording
- Local STT and TTS on a Raspberry Pi 5, with local Ollama conversation by default
- Optional low-latency Gemini conversation with automatic Ollama fallback
- Streamed response playback without requiring PSRAM
- Ember-driven expressions and sleep, wake, mute, and unmute actions
- Accepted speech output level of 141

The M5Stack's PSRAM currently fails its boot-time hardware test, but PSRAM is
optional: the implemented face, voice, networking, and playback features use
bounded internal-RAM buffers and remain operational.

## Voice stack

The Raspberry Pi gateway keeps voice processing on the local network:

- whisper.cpp `base.en` for speech recognition
- Ollama `llama3.2:3b` for private local conversation
- Optional Gemini `gemini-3.5-flash-lite` for faster cloud conversation
- Piper `en_GB-alba-medium` for Ember's voice
- FastAPI gateway for authentication, local commands, and response audio

No cloud AI account is required. Gemini is opt-in; only transcribed prompt text is sent
to Google, while speech recognition and voice generation remain on the Pi.

## Build and upload

The project uses PlatformIO with the Arduino framework:

```powershell
pio run
pio run --target upload --upload-port COMXX
pio device monitor --port COMXX --baud 115200
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

## License

Fire-chan's original code and documentation are licensed under the
[Apache License 2.0](LICENSE), copyright 2026 gflerm. Third-party libraries,
tools, models, and voice assets retain their own licenses; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) before redistributing source,
firmware binaries, models, or prepared Raspberry Pi images.
