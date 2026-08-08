# Fire-chan Development Roadmap

**Created:** 2026-08-04

**Current firmware:** `0.11.1-volume-status`

**Current gateway experiment:** Optional Gemini `gemini-3.5-flash-lite` conversation
with minimal thinking and automatic Ollama fallback; 8 kHz canonical WAV replies
(`EMBER_AUDIO_RATE_HZ=8000`) active on the Pi. Streaming early-start playback
attempted twice and rolled back; re-attempt requires a serial capture.

**Principle:** Preserve a responsive, private, useful companion even when the internet
or optional PSRAM is unavailable.

This is the active implementation backlog. Verified results belong in
`PROJECT_PROGRESS.md`; architectural choices belong in `DECISIONS.md`.

## Priority 0 — Complete the current assistant loop

- [ ] Physically test “go to sleep”, “wake up”, “mute”, and “unmute” end to end.
- [ ] Confirm each returned emotional hint appears after speech and expires cleanly.
- [ ] Run at least 25 consecutive prompts while watching heap, frame rate, audio, and SD use.
- [ ] Test Pi unavailable, Wi-Fi loss, timeout, malformed response, and recovery without reboot.
- [ ] Support cancelling an in-flight request and stopping speech with a button.
- [ ] Decide how a new push-to-talk request interrupts current playback.
- [ ] Add bounded diagnostic counters for successful, failed, cancelled, and timed-out turns.

## Priority 1 — Device-friendly Wi-Fi and gateway setup

- [ ] Enter setup mode automatically when no valid Wi-Fi configuration exists.
- [ ] Allow setup mode from a deliberate button combination at boot.
- [ ] Start a clearly named Fire-chan configuration access point and captive portal.
- [ ] Show simple on-device instructions, setup progress, success, and failure states.
- [ ] Scan for nearby networks and allow manual SSID entry for hidden networks.
- [ ] Collect Wi-Fi password, Pi hostname or address, gateway port, and shared token.
- [ ] Validate credentials before saving; keep the portal available when validation fails.
- [ ] Store credentials in NVS and remove runtime dependence on `include/secrets.h`.
- [ ] Never write passwords or tokens to serial logs, the SD backup, or the web page response.
- [ ] Add reconnect backoff, last-known network selection, and an offline fallback.
- [ ] Provide a safe “forget network” flow requiring physical confirmation.
- [ ] Prefer Pi hostname discovery or mDNS so a DHCP address change does not break Ember.
- [ ] Add setup documentation and recovery instructions.

Acceptance criteria:

- A first-time user can configure Fire-chan from a phone without rebuilding firmware.
- Incorrect credentials can be corrected without erasing firmware.
- Secrets survive restart but never appear in source control or ordinary logs.
- Fire-chan remains expressive and usable offline.

## Priority 2 — Multi-turn conversation

- [x] Assign a stable device/session identifier without exposing private hardware identifiers.
- [x] Keep a bounded recent-turn history on the Pi, not on the ESP32.
- [x] Resolve follow-ups such as “What about tomorrow?” using recent context.
- [x] Add an explicit “forget our conversation” command and automatic expiry.
- [x] Start a fresh session after a configurable idle period.
- [x] Prevent command responses and model replies from duplicating each other.
- [x] Keep replies short enough for a desktop companion and allow “tell me more”.
- [x] Preserve Ember's name, warm tone, and current voice across sessions.
- [x] Ground time, date, device state, and other factual local commands in tools rather than guesses.
- [x] Measure turn latency for recording, STT, model response, TTS, download, and playback.
- [ ] Compare at least ten identical prompts through Gemini and Ollama using gateway timings.
- [x] Re-measure the corrected 4KB audio-download batching with a controlled short reply.
- [ ] Reduce the 3–5 second Fire request overhead before changing STT or TTS models.
- [ ] Evaluate playback from a growing/streamed WAV so speech can start before the full download
      (attempted 0.12.0-stream-play and 0.12.0-stream-3buf; both rolled back — capture the failure
      on serial before retrying).
- [ ] Evaluate 16 kHz response audio or a compact speech codec only if quality remains acceptable
      (8 kHz canonical WAV works and is active on the Pi; 16 kHz still untested).

Acceptance criteria:

- Ember correctly handles at least five related turns in one session.
- A reset or expired session does not silently restore old private conversation.
- Local commands remain deterministic regardless of conversation history.

## Priority 3 — Essential assistant abilities

### Device and conversation control

- [x] Offer concise help: “What can you do?”
- [x] Repeat the last response.
- [x] Change volume by intent or percentage with safe clamp to 0–100 (gateway
      commands emit `volume=N` or relative `volume=+N`/`volume=-N`; the Fire
      applies and persists them to NVS; live device verification pending).
- [x] Report Wi-Fi, Pi connectivity, battery state, mute state, free storage, and
      firmware version (Fire uploads `X-Ember-Device-Status` facts with every
      turn; the gateway grounds the "status" reply from them).
- [ ] Change expression, play a named sound, sleep, wake, mute, and unmute.
- [ ] Stop speaking.
- [ ] Recognize uncertainty and say when a request cannot be completed.

### Time, timers, and reminders

- [ ] Add deterministic date, day, and timezone answers.
- [ ] Create, list, cancel, pause, resume, and name timers.
- [ ] Add reminders with persistent storage and clear confirmation of interpreted time.
- [ ] Provide alarm ringing, snooze, dismiss, and missed-reminder behavior.
- [ ] Survive restart, Wi-Fi loss, and Pi downtime without losing committed reminders.
- [ ] Define daylight-saving and timezone-change behavior.

### Information and utility

- [ ] Add local weather only after location consent and a clearly selected data source.
- [ ] Add basic calculations and unit conversions through deterministic tools.
- [ ] Provide calendar integration only as an optional, separately authorized feature.
- [ ] Add configurable morning/evening summaries with a physical or web disable control.

## Priority 4 — Local control, maintenance, and reliability

- [ ] Build a local status/configuration web interface.
- [ ] Add an authenticated HTTP API matching the design specification.
- [ ] Implement signed or otherwise safely verified OTA firmware updates with rollback.
- [ ] Add safe mode for repeated boot failures or corrupt configuration.
- [ ] Add SD sound packs and reusable alarm playback with fallback built-in cues.
- [ ] Rotate bounded logs and provide an export that excludes credentials and conversation audio.
- [ ] Remove expired cached prompt and response audio according to a documented retention policy.
- [ ] Run overnight and multi-day stability tests with reconnect and Pi-restart scenarios.
- [ ] Investigate battery voltage reporting before using it for low-battery decisions.
- [ ] Continue PSRAM diagnosis separately; never make it mandatory for core features.

## Priority 5 — Privacy, safety, and trust

- [ ] Keep push-to-talk as the default and make recording visually obvious.
- [ ] Document exactly where audio, transcripts, history, and generated speech are stored.
- [ ] Add configurable deletion and retention controls on the Pi.
- [ ] Require physical confirmation for credential reset, factory reset, restart, OTA, and other disruptive actions.
- [ ] Require explicit authorization before adding calendars, messaging, smart-home, or cloud services.
- [ ] Separate informational replies from confirmed actions: never claim an action succeeded without a device result.
- [ ] Sanitize assistant output before turning it into device actions.
- [ ] Rate-limit authentication failures and sensitive local endpoints.
- [ ] Add a privacy mode that disables recording and clears pending audio.
- [ ] Evaluate a local wake word only after interruption, privacy, and disable controls are proven.

## Priority 6 — Personality and polish

- [ ] Refine Ember's pace and prosody while retaining accepted speech level 141.
- [ ] Add configurable quiet hours and softer nighttime cues.
- [ ] Add personality profiles without allowing them to override safety rules.
- [ ] Synchronize mouth movement more closely with response audio amplitude.
- [ ] Add subtle listening, thinking, network-wait, and failure animations.
- [ ] Support user-selected names and voices through validated profiles.
- [ ] Add an optional first-run introduction and capability tour.

## Later integrations

- [ ] MQTT and Home Assistant with an explicit allowlist of permitted actions.
- [ ] External notifications with quiet hours, priority, acknowledge, and dismiss behavior.
- [x] Add Gemini as a replaceable conversation provider without making cloud inference required.
- [ ] Evaluate optional cloud STT or TTS only if local quality or latency becomes inadequate.
- [ ] Optional companion phone or desktop setup interface.
- [ ] Backup and restore of non-secret configuration.

## Release gate for the next milestone

- [x] Apache 2.0 project licensing and principal third-party notices are published.
- [ ] Device-friendly provisioning passes first-use and recovery tests.
- [ ] Five-turn contextual conversation passes without breaking deterministic commands.
- [ ] Voice cancellation and interruption are predictable.
- [ ] Twenty-five-turn and overnight tests show no progressive heap loss or face freeze.
- [ ] Credentials and private content do not appear in Git, serial logs, or exported diagnostics.
- [ ] Progress, decisions, setup guide, and user controls are documented.
