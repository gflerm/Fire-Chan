# Fire-chan Development Roadmap

**Current firmware:** `0.12.0-alarm-ringing`
**Current gateway:** `0.7.2` (Raspberry Pi 5; whisper.cpp + Ollama + Piper;
optional Gemini with Ollama fallback; 8 kHz canonical WAV replies)

**Principle:** Stay responsive, private, and useful even when the internet
or optional PSRAM is unavailable.

Verified results live in `PROJECT_PROGRESS.md`; architecture choices in
`DECISIONS.md`; session log in `opencode_update.md`.

## Priority 0 — Complete the current assistant loop

- [ ] Physically test "go to sleep", "wake up", "mute", and "unmute" end to end.
- [ ] Confirm each returned emotional hint appears after speech and expires cleanly.
- [ ] Run at least 25 consecutive prompts while watching heap, frame rate, audio, and SD use.
- [ ] Test Pi unavailable, Wi-Fi loss, timeout, malformed response, and recovery without reboot.
- [ ] Support cancelling an in-flight request and stopping speech with a button.
- [ ] Decide how a new push-to-talk request interrupts current playback.
- [ ] Add bounded diagnostic counters for successful, failed, cancelled, and timed-out turns.

## Priority 1 — Device-friendly Wi-Fi and gateway setup

- [ ] Enter setup mode automatically when no valid Wi-Fi configuration exists.
- [ ] Allow setup mode from a deliberate button combination at boot.
- [ ] Run a clearly named Fire-chan configuration access point and captive portal.
- [ ] Show on-device instructions, setup progress, success, and failure states.
- [ ] Scan for nearby networks and allow manual SSID entry for hidden networks.
- [ ] Collect Wi-Fi password, Pi hostname or address, gateway port, and shared token.
- [ ] Validate credentials before saving; keep the portal available when validation fails.
- [ ] Store credentials in NVS and remove runtime dependence on `include/secrets.h`.
- [ ] Never write passwords or tokens to serial logs, the SD backup, or any web response.
- [ ] Add reconnect backoff, last-known network selection, and an offline fallback.
- [ ] Provide a safe "forget network" flow requiring physical confirmation.
- [ ] Prefer Pi hostname discovery or mDNS so a DHCP address change does not break Ember.
- [ ] Add setup documentation and recovery instructions.

Acceptance:

- First-time user can configure Fire-chan from a phone without rebuilding firmware.
- Incorrect credentials can be corrected without erasing firmware.
- Secrets survive restart but never appear in source control or ordinary logs.
- Fire-chan remains expressive and usable offline.

## Priority 2 — Multi-turn conversation

- [x] Stable device/session identifier without exposing private hardware identifiers.
- [x] Bounded recent-turn history on the Pi.
- [x] Follow-ups resolved against recent context.
- [x] Explicit "forget our conversation" command and automatic expiry.
- [x] Fresh session after a configurable idle period.
- [x] Command responses and model replies do not duplicate each other.
- [x] Short replies with "tell me more".
- [x] Ember's name, warm tone, and current voice persist across sessions.
- [x] Time, date, device state, and other local facts come from tools, not guesses.
- [x] Per-stage turn-latency metrics (recording, STT, model, TTS, download, playback).
- [ ] Compare at least ten identical prompts through Gemini and Ollama using gateway timings.
- [x] Re-measure the corrected 4 KB audio-download batching with a controlled short reply.
- [ ] Reduce the 3–5 second Fire request overhead before changing STT or TTS models.
- [ ] Evaluate playback from a growing/streamed WAV (0.12.0-stream-play and
      0.12.0-stream-3buf both rolled back — capture the failure on serial before retrying).
- [ ] Evaluate 16 kHz response audio or a compact codec only if quality remains
      acceptable (8 kHz canonical WAV works on the Pi; 16 kHz still untested).

## Priority 3 — Essential assistant abilities

### Device and conversation control

- [x] Concise help ("What can you do?").
- [x] Repeat the last response.
- [x] Change volume by intent or percentage (clamped 0–100); Fire applies and persists to NVS.
- [x] Report Wi-Fi, Pi connectivity, battery, mute, free storage, and firmware (via
      `X-Ember-Device-Status`).
- [ ] Change expression, play a named sound, sleep, wake, mute, and unmute.
- [ ] Stop speaking.
- [ ] Recognize uncertainty and say when a request cannot be completed.

### Time, timers, and reminders

- [x] Deterministic date, day, and timezone answers.
- [x] Create, list, cancel, and name timers.
- [ ] **Pause and resume timers** — _deferred._
- [x] Reminders with persistent storage and clear confirmation of interpreted time.
- [ ] **Alarm ringing, snooze, dismiss, and missed-reminder behavior** —
      ringing + dismiss done; **snooze and missed-reminder behavior still needed**.
      Also: confirm the timer (not alarm) path is end-to-end verified, and the
      timer→alarm distinction is documented for the user.
- [x] Survive restart, Wi-Fi loss, and Pi downtime without losing committed reminders.
- [ ] Define daylight-saving and timezone-change behavior.

### Information and utility

- [x] Local weather only after location consent and a clearly selected data source
      (Open-Meteo; named-place geocoding; IP-based auto-detect with stored-location
      persistence).
- [x] Grounded web search answers from DuckDuckGo results.
- [x] Basic calculations and unit conversions through deterministic tools
      (safe AST-evaluated arithmetic; length/mass/volume/speed; C/F/K).
- [ ] Calendar integration (optional, separately authorized).
- [ ] Configurable morning/evening summaries with a physical or web disable control.

## Priority 4 — Local control, maintenance, and reliability

- [ ] Local status/configuration web interface.
- [ ] Authenticated HTTP API matching the design specification.
- [ ] Signed or otherwise safely verified OTA firmware updates with rollback.
- [ ] Safe mode for repeated boot failures or corrupt configuration.
- [ ] SD sound packs and reusable alarm playback with fallback built-in cues.
- [ ] Rotate bounded logs; export must exclude credentials and conversation audio.
- [ ] Remove expired cached prompt and response audio per a documented retention policy.
- [ ] Overnight and multi-day stability tests with reconnect and Pi-restart scenarios.
- [ ] Investigate battery voltage reporting before relying on it for low-battery decisions.
- [ ] Continue PSRAM diagnosis separately; never make it mandatory.

## Priority 5 — Privacy, safety, and trust

- [ ] Keep push-to-talk as the default and make recording visually obvious.
- [ ] Document exactly where audio, transcripts, history, and generated speech are stored.
- [ ] Configurable deletion and retention controls on the Pi.
- [ ] Physical confirmation for credential reset, factory reset, restart, OTA, and disruptive actions.
- [ ] Explicit authorization before adding calendars, messaging, smart-home, or cloud services.
- [ ] Separate informational replies from confirmed actions: never claim success without a device result.
- [ ] Sanitize assistant output before turning it into device actions.
- [ ] Rate-limit authentication failures and sensitive local endpoints.
- [ ] Privacy mode that disables recording and clears pending audio.
- [ ] Evaluate a local wake word only after interruption, privacy, and disable controls are proven.

## Priority 6 — Personality and polish

- [ ] Refine Ember's pace and prosody while keeping accepted speech level 141.
- [ ] Configurable quiet hours and softer nighttime cues.
- [ ] Personality profiles that cannot override safety rules.
- [ ] Synchronize mouth movement more closely with response audio amplitude.
- [ ] Subtle listening, thinking, network-wait, and failure animations.
- [ ] User-selected names and voices through validated profiles.
- [ ] Optional first-run introduction and capability tour.

## Later integrations

- [ ] MQTT and Home Assistant with an explicit allowlist of permitted actions.
- [ ] External notifications with quiet hours, priority, acknowledge, dismiss.
- [x] Gemini as a replaceable conversation provider (cloud inference is optional).
- [ ] Optional cloud STT/TTS only if local quality or latency becomes inadequate.
- [ ] Companion phone or desktop setup interface.
- [ ] Backup and restore of non-secret configuration.

## Release gate for the next milestone

- [x] Apache 2.0 project licensing and principal third-party notices published.
- [ ] Device-friendly provisioning passes first-use and recovery tests.
- [ ] Five-turn contextual conversation passes without breaking deterministic commands.
- [ ] Voice cancellation and interruption are predictable.
- [ ] Twenty-five-turn and overnight tests show no progressive heap loss or face freeze.
- [ ] Credentials and private content do not appear in Git, serial logs, or exported diagnostics.
- [ ] Progress, decisions, setup guide, and user controls are documented.