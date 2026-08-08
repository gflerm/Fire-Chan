# Fire-chan OpenCode Update Log

Session notes recorded by OpenCode as the TODO roadmap is worked through.

## 2026-08-07 — Multi-turn conversation, Part 1: stable device identity

Firmware `0.10.0-device-id`. Started Priority 2 (multi-turn conversation) scope A:
give Fire-chan a stable, anonymous per-device identity so the Pi gateway can later
keep bounded per-device conversation history.

- [x] Added an anonymous 32-hex-char `deviceId` to `AppConfig`.
- [x] Generate the id once via `esp_random()` on first boot and persist it in NVS
      through the existing `ConfigManager` (survives restarts, config changes).
- [x] Wrote `deviceId` into the human-readable `/config/device.json` SD backup.
- [x] `VoiceGatewayClient` now sends `X-Ember-Device` on `/v1/voice` uploads
      (falls back to `local` when unset).
- [x] `Application` plumbs the config device id into the gateway client.
- [x] Built and uploaded firmware `0.10.0-device-id` to the Fire on COMxx.
- [x] Physical verification: id generated once, reloaded from NVS on a later boot
      (no regeneration), microSD backup written, Wi-Fi join and face loop unaffected.

Verified boot log excerpt:

```
[CONFIG] NVS settings loaded
[CONFIG] deviceId=4f3b309...   (id persisted; no "generated" line on repeat boot)
[CONFIG] microSD=ready backup=ok
```

Backwards compatible: the running gateway is unchanged and treats any request
without a device header as a single implicit session.

Next: gateway-side bounded multi-turn history keyed by this device id, with an
in-memory session store, idle expiry, a "forget" command, and deterministic
grounded date/time commands.

## 2026-08-07 (continued) — Multi-turn conversation, Part 2: gateway history

Gateway `0.3.0`. Added per-device conversation memory so follow-ups resolve against
recent context, with history held only on the Pi.

- [x] New `memory.SessionStore`: in-memory, per-device, bounded recent turns, idle
      expiry, and an explicit `clear` for the "forget" command.
- [x] Refactored `ConversationProvider.chat()` to accept a full message thread
      (system + alternating user/assistant) instead of a single transcript.
      Ollama sends it verbatim; Gemini splits the system message into
      `systemInstruction` and maps assistant turns to `model`. Fallback unchanged.
- [x] `main.py` reads the anonymous `X-Ember-Device` header (older firmware without
      it falls back to one implicit "default" session). It builds the thread from
      stored history, records each user/assistant turn, and clears on the reset
      command. `services.chat()` passes the message list through.
- [x] Local commands resolve before the model and record exactly one reply, so
      command results and model replies cannot duplicate each other.
- [x] Added a deterministic "forget/clear/start over" command and a grounded
      date/day command (timezone clock), both resolved locally.
- [x] New configurable `SESSION_MAX_TURNS=6` and `SESSION_IDLE_SECONDS=1800` in
      `config.py` and `.env.example`; fresh session after idle gap.
- [x] Reinforced `personality.txt`: build on the previous reply when asked
      "tell me more"; name, tone, and voice already persist unchanged.
- [x] Tests: new `test_memory.py` (bounded rollover, idle reset, clear, isolation),
      updated `test_llm.py` for cross-thread mapping + fallback,
      `test_commands.py` for forget and date. All 16 gateway tests pass.

Acceptance mapping: max turns defaults to 6 (>= 5 related turns); in-memory history
and idle/forget reset mean an expired session never silently restores old private
conversation; commands stay deterministic because they bypass history entirely.

Deployment (on the Pi): `cd gateway && chmod +x scripts/update-pi.sh && sudo ./scripts/update-pi.sh`,
then verify a five-turn exchange with `X-Ember-Device` set.

Remaining Priority 2 scope A: live multi-turn verification on hardware, then the
deferred latency-optimization milestones.

## 2026-08-07 (continued) — Deployment and verification draft

- [x] Created `MULTI_TURN_DEPLOY_VERIFY.md` in the repo: a deployment + verification
      draft for Priority 2 scope A.
- [x] Steps: deploy gateway with `sudo ./scripts/update-pi.sh` (token and provider
      config in `/etc/ember/ember.env` are preserved); optional
      `SESSION_MAX_TURNS`/`SESSION_IDLE_SECONDS` knobs; firmware is already on the
      Fire so no firmware redeploy is needed.
- [x] Verification matrix: live `curl` test with `X-Ember-Device`, five related
      turns, deterministic date, "forget"/"start over" reset, session isolation
      per device id, idle-gap fresh session, and gateway-restart clearing.
- [x] Re-checked guards: anonymous random id (never MAC/serial), no secrets logged
      or committed, local commands bypass history to stay deterministic.
- [ ] Live end-to-end verification on hardware (pending user execution on the Pi).

## 2026-08-07 (continued) — Gateway 0.3.0 deployed on the Pi

- [x] Set up passwordless SSH to the Pi: new `id_ed25519` key at
      `C:\Users\glerm\.ssh\id_ed25519` (no passphrase), public key authorized for
      `georg@xxx.xxx.xxx.xxx`.
- [x] Transferred `gateway/` to `/home/georg/fire-chan-gateway` on the Pi (cleaned
      up a duplicated nested copy left by an earlier timed-out transfer).
- [x] Ran `sudo ./scripts/update-pi.sh`: restarted `ember-gateway`, installed the
      package into `/opt/ember`; dependencies already satisfied.
- [x] Health after deploy: `{"ok":true,"components":{"whisper":true,
      "piper":true,"conversation":true},"conversation_provider":"gemini"}`.
- [x] Verified running install: `__version__ = "0.3.0"`, `memory.py` present,
      `SESSION_MAX_TURNS`/`SESSION_IDLE_SECONDS` config present, service `active`.
- [ ] Remaining: live multi-turn hardware test (recorded prompts + device header).

## 2026-08-07 (continued) — Priority 2 scope B: controlled latency baseline

- [x] Added `gateway/scripts/latency-bench.py`: a controlled, reproducible baseline
      that synthesizes a fixed 68 KB 16 kHz speech control clip (Piper + ffmpeg)
      and posts it to `/v1/voice` N times with a fresh device id per rep, printing
      per-stage timings and response audio bytes. stdlib-only, run on the Pi.
- [x] Explicitly measured a controlled short-reply baseline (the goal of scope B's
      "corrected 4 KB download-batching" and "ten identical prompts" bullets).

Medians/avatars of 6 identical controlled turns (provider flipped live and restored):

| Stage | Gemini | Ollama |
|---|---|---:|
| transcription | ~2.0 s | ~2.0 s |
| conversation | ~0.95 s | ~8.1 s |
| synthesis | ~0.93 s | ~1.1 s |
| gateway_total | ~4.0 s | ~11.4 s |

- Gemini gateway return is ~2.9x faster than Ollama on the same input.
- Even on Gemini the ~4 s is ~50% whisper transcription + ~25% conversation
  + ~25% Piper synthesis.
- Response WAV bytes follow reply length: ~100-140 chars -> ~250-640 KB @ 22.05k
  mono. On the Fire this download is the dominant end-to-end slice.
- Provider left as `gemini`, health OK after the comparison.

Pending: capture Fire-side [LATENCY] (fire_request + audio_download + ready_total)
for the same controlled short reply on the physical unit to isolate the Fire
request vs download overhead.

Fire-side live capture (controlled short command, two turns; then a Gemini turn):

| Metric | Cmd turn 1 | Cmd turn 2 | Gemini turn |
|---|---:|---:|---:|
| gateway_total | 2336 ms | 2257 ms | 3559 ms |
| fire_request | 5011 ms | 4912 ms | 6466 ms |
| audio_download (78/78/193 KB) | 3455 ms | 3065 ms | 6440 ms |
| ready_total | 8466 ms | 7977 ms | 12906 ms |

Isolation conclusions:

- Fire request overhead ~2.9 s, reply-independent (network round-trip + HTTP
  multipart + JSON parse on the ESP32).
- Audio download ~ 0.13-0.16 s/KiB (~28 KB/s). The dominant, size-dependent cost:
  78 KiB -> ~3 s, 193 KiB -> 6.4 s, a 300-640 KiB reply -> 10-20 s.
- whisper transcription ~2 s is inside gateway_total and is a fixed base cost.
- Transients seen: one mic `capture buffer overrun` and an SD `no token received`
  during streamed playback (both non-fatal; noted for stability follow-up).

Implication for scope B: audio-download throughput (and streaming/early playback)
is the largest lever; the corrected 4 KB batching currently yields only ~28 KB/s
effective, so it is the priority to re-measure and improve.

### Audio-download bottleneck — measured (firmware 0.11.0-download-conn)

Hypothesized and isolated with firmware experiments on the physical unit:

- The server DOES send `content-length` (verified with curl), but the client logs
  `length=-1`; ArduinoHttpClient falls into read-until-close mode. That alone is not
  the delay (connection closes promptly with `Connection: close`).
- Tried `Connection: close` + removing the premature `setNoDelay` (fixing a
  `setsockopt fd=-1` error). Tried enlarging the SD batch buffer 4 KB -> 16 KB.
- Measured on-device:
  - 78 KB reply: audio_download ~3.1 s (~26 KB/s) at both 4 KB and 16 KB buffers.
  - 159 KB reply: ~5.2 s (~31 KB/s).
- Conclusion: download throughput is flat ~28-31 KB/s regardless of buffer/target
  size -> network receive-window limited on the ESP32 (small lwIP TCP window x WIFi
  RTT), not an SD-write or keep-alive stall. Growing the batch buffer does not help.
- Kept as the net change: `Connection: close` on both requests (clean end-of-body);
  reverted the buffer to 4 KB to preserve heap. `set_nodelay` removed.
- Implication: the meaningful lever for long replies is to start playback from a
  growing/streamed WAV (hear speech start well before the full download) or to
  shrink response audio bytes (16 kHz / codec), both of which are measured open
  items in TODO Priority 2.

### Audio output rate: env-tunable (was fixed 22.05 kHz via Piper)

Piper `en_GB-alba-medium` emits 22050 Hz mono; the firmware WAV player already
honors the header rate across 8000-48000 Hz (`ResponseAudioPlayer.cpp:128`), so the
reply rate is tunable gateway-side only.
- Added `EMBER_AUDIO_RATE_HZ` (default 16000, floor 4000):
  - `gateway/ember_gateway/services.py`: `resample_to()` uses `ffmpeg -ar <rate>
    -ac 1` and falls back to Piper's raw bytes if ffmpeg is absent/fails; applied in
    `LocalVoiceServices.synthesize`. Rate threaded through the service constructor.
  - `config.py`: `audio_rate_hz` from env. `main.py`: passes it to the service.
  - `.env.example`: documented the toggle. `install-pi.sh`: added `ffmpeg` to apt deps.
- Record (mic) stays fixed 16 kHz: Whisper STT expects 16 kHz input.

### 8 kHz and 16 kHz field test: FAILED, reverted to Piper-native 22.05 kHz

Deployed to the Pi (xxx.xxx.xxx.xxx) with `EMBER_AUDIO_RATE_HZ=8000` and then `16000`.
BOTH rates produced a red error screen on the Fire during the turn (device-side),
so the sample-rate resampling is reverted.
- Rollback = exact prior working state: removed the ffmpeg resample from
  `services.synthesize`, removed `audio_rate_hz` from `config.py`/`main.py`/`.env`,
  deleted the env var from `/etc/ember/ember.env`, redeployed, health OK.
- Note: the red error screen is so far UNCAPTURED on serial (monitor runs saw no
  data - no active turn during the windows). Because both 8 k and 16 k fail while
  22.05 k works, the common new factor is the ffmpeg-rewritten WAV header; a likely
  firmware parse/validation reject (e.g. non-44-byte RIFF header, extra/fmt/`data`
  chunk layout, or blockAlign mismatch). NEEDS a serial capture to confirm.
- OPEN: capture the red error screen on the wire before any further audio-format
  change (monitor during a real turn).

### Confirmed working again on device

User re-tested after the Piper-native rollback (22.05 kHz): voice turns are
working again. The 8 kHz / 16 kHz format experiments are shelved; the resample
change is fully reverted on both the Pi and the local source. The audio-format
experiment remains OPEN pending a serial capture of the red error before retrying.

### Unblocked: canonical-header 8 kHz works, streaming now feasible

Root cause of the earlier 8 kHz / 16 kHz red screen: the ffmpeg-resampled WAV
layout. Rewrote the gateway resampler to emit a STRICT 44-byte canonical PCM WAV
(`canonical_pcm_wav`: s16le + RIFF/fmt/data header, blockAlign=2, bytesPerSec set),
instead of ffmpeg's `-f wav` output. This matches exactly what the Fire's parser
validates.
- `services.py`: `resample_to()` -> ffmpeg `-f s16le -ar <rate>` then wrap in
  canonical header. `audio_rate_hz` threaded via config (`EMBER_AUDIO_RATE_HZ`,
  default `0` = Piper-native passthrough; floor 0). `main.py` passes it.
- Deployed to Pi; test at 8000 Hz on device:
  - `[PLAYBACK] started rate=8000Hz ... bytes=22664` -- parses and plays (no error).
  - Same "What time is it?" reply: bytes 78 KB(22.05k) -> 22.1 KB(8k), and
    `audio_download` 3.1 s -> 1.48 s (~2.1x NT). `ready_total` ~6.8 s dominated by
    `fire_request` (5.3 s).
- Streaming is now VIABLE: at 8 kHz real-time playback = 16 KB/s while download
  runs ~28 KB/s (~1.75x head), so early-start playback can run without starving
  (this was BLOCKED at 22.05 kHz where play 44 KB/s > download 28 KB/s).
- Rec: keep `EMBER_AUDIO_RATE_HZ=8000` on the Pi; awaiting the 8 kHz sound-quality
  verdict, then implement streamed early-start playback via a RAM ring buffer fed
  from the download, consuming to M5.Speaker (avoiding concurrent SD read/write).

### Streamed early-start playback: attempt FAILED (red error), rolled back

Implemented on branch `oc-updates` as firmware 0.12.0-stream-play:
- New `src/audio/AudioStreamSink.h` interface (producer -> sink).
- `ResponseAudioPlayer` gained a streaming mode: a 32 KB RAM ring fed by the
  gateway, plus `performStreamPlayback()` which parses the WAV header from the
  ring and feeds M5.Speaker live (`StreamPending/StreamPlaying` states).
- `VoiceGatewayClient` pushes each downloaded batch into the sink while also
  writing to SD; `beginStream/streamWrite/endStream` driven from its task.
- `Application` wires the sink, detects streaming start to publish
  `SpeakingStarted`/suspend the mic, and applies the directive on `Finished`
  (or immediately if muted / not streaming).
- Build clean; flashed; user observed a RED ERROR screen on the turn -> reverted
  to the last known-good firmware 0.11.0-download-conn (git checkout 557bf12),
  reflashed, confirmed working again.
- OPEN (do NOT re-attempt blind): capture the streaming-turn red error on serial
  first. Candidates to check next time: M5.Speaker begin while a previous
  playback is active, ring starvation causing `pullBytes` timeout to trip a
  false failure path, or speaker task interleaving with the gateway download
  task. Since the 8 kHz canonical WAV path itself works, the red screen is
  specific to the streaming build.
- Repo state: firmware source fully reverted; gateway (Pi + source) still at
  `EMBER_AUDIO_RATE_HZ=8000` with the canonical-header resampler (kept).

## 2026-08-08 — Streaming v2 attempt, runtime volume command, rollback

Picked up from the "next steps" of the 0.12.0-stream-play rollback; goal was to get a
serial capture of the streaming failure before retrying.

- Streamed early-start playback v2 (`0.12.0-stream-3buf`) on `oc-updates`:
  new `src/audio/StreamSink.h` producer->sink interface; `ResponseAudioPlayer`
  implemented it with a 16 KB ring under a mutex, `StreamStarted` event, and
  `StreamPending/StreamPlaying` states, feeding M5.Speaker from three static
  1536-byte buffers (never handing ring pointers to the speaker);
  `VoiceGatewayClient` gained `setStreamSink()`, `usedStream()`, and per-download
  `beginStream/streamWrite/endStream/abortStream`; `Application` attached the sink
  per turn and handled the new start/finish/fail events. Build clean
  (RAM 2.1%, Flash 16.6%).
- Runtime volume: added a serial `volume [0-100]` command
  (`Application::handleSerialCommand` + `AudioFeedback::setVolumePercent`), rebuilt,
  flashed, and applied `volume 75` (from 65). Confirmed live:
  `[AUDIO] volume set to 75% speech_level=162`, persisted `nvs=ok sd=ok`.
- Device LOCKUP on the first real streamed turn: face stuck `Speaking`, no audio,
  no button response, stable heap; key insight — every earlier live turn used the
  file-playback path (`[PLAYBACK] started`), never `[PLAYBACK] stream started`, so
  the streaming path had never completed a real turn. Streaming-specific failure;
  8 kHz canonical WAV playback itself works.
- Rollback: reverted the uncommitted volume experiment (working tree restored),
  `git revert 7aed648` -> commit `c63983d`, rebuilt and flashed known-good
  `0.11.0-download-conn`; device verified stable (boot, Wi-Fi, a live turn).
  Revert pushed to origin/oc-updates. `main` untouched.
- Lessons for the next attempt: the streaming path must be exercised on-device
  with a serial capture from the very first streamed turn; candidates remain
  M5.Speaker begin-while-active, ring starvation, and speaker/gateway task
  interleaving on core 0. The 0.12.0-stream-3buf commit and its revert are both
  in history for reference.

Docs updated: README, PROJECT_PROGRESS (last updated 2026-08-08), TODO.md
(Priority 2 status: scope A done; scope B remaining open items), gateway/README,
MULTI_TURN_DEPLOY_VERIFY status note, and this log. File cleanup: renamed
`future-enhancements.md.md` -> `future-enhancements.md` and `future-updgrade.md` ->
`future-upgrade.md`.

## 2026-08-08 (continued) — Priority 3 slice 1: gateway help / repeat / volume

- Commands in `gateway/ember_gateway/commands.py`:
  - `help` / "What can you do?" -> capability listing, `action="help"`.
  - `repeat` / "Say that again" -> `repeat_last=True`; the handler replays the
    newest stored assistant reply from `SessionStore.last_reply()`, or says there
    is nothing to repeat yet.
  - Volume intents: "set the volume to N" (clamped 0-100) -> `volume=N`;
    "turn it up / louder" -> `volume=+10`; "turn it down / quieter" -> `volume=-10`.
    The Fire owns the current level and clamps relative steps, keeping responses
    deterministic and testable without device involvement.
- `memory.py`: added `SessionStore.last_reply(device_id)`.
- `main.py`: repeat path wired through the `/v1/voice` handler.
- Gateway version bumped to `0.4.0`.
- Tests: 25 pass (`test_commands` +5 help/repeat/volume, `test_memory` +3 last_reply,
  plus existing). Run locally with `PYTHONPATH=gateway python -m unittest discover -s tests`.
- Not committed yet; device-side application of `volume=` actions and status facts
  remain Priority Three slice 2.

## 2026-08-08 (continued) — Priority 3 slice 2: Fire volume + device status

- Fire `0.11.1-volume-status` (flashed and boot-verified):
  - `AudioFeedback::setVolumePercent` re-added (was reverted with the streaming
    experiment); `begin()` now delegates to it.
  - `AssistantDirective` gained `AssistantAction::Volume` plus
    `hasVolume/volumeAbsolute/volumeTarget/volumeDelta`; `parseVolume` accepts
    `volume=40`, `volume=40%`, `volume=+10`, `volume=-10`.
  - `Application::setAudioVolume` clamps 0-100, applies to the speaker, persists
    `config_.volumePercent` (NVS + SD backup). Applied after Speech finishes,
    like mute.
  - `Application::buildDeviceStatus` uploads `X-Ember-Device-Status` every turn
    (`fw=...;wifi=...;sd_free_mb=...;battery=...`); `ConfigManager` gained
    `sdFreeBytes()/sdTotalBytes()`.
- Gateway `0.4.1`: `parse_device_status` + `_status_describe` in commands.py;
  `/v1/voice` reads `X-Ember-Device-Status` and the "status"/"how are you" reply
  is grounded in those facts when present. 30 tests passing.
- Boot capture on device: `[CONFIG] volume=75% muted=false`, Wi-Fi online
  (`xxx.xxx.xxx.xxx`, rssi=-54), gateway task ready, PSRAM test fail as documented
  (optional PSRAM not fitted). Stable heap while idle.
- Definition of done for slice 2: a live spoken volume turn through the gateway
  and a confirmed persisted value. Not yet physically verified on this session.

- Status-fact question follow-up: when asked questions like "what is your battery?"
  or "Wi-Fi?", Ember answered "I don't know" because the matcher only fired on
  "status"/"how are you" and the free-storage fact was never rendered. Gateway now
  matches battery/Wi-Fi/storage/firmware/levels intents, renders `sd_free_mb`, and
  omits `na` values. REMEMBER: the Pi must be updated (`sudo ./scripts/update-pi.sh`)
  after every gateway change; the running stack was last built before slices 1-2.

## 2026-08-08 (continued) — Priority 3 slice 3: gateway tools (search / weather / timers)

- New `gateway/ember_gateway/search.py`: `WebSearchClient` calls the DuckDuckGo
  Instant Answer API (`api.duckduckgo.com`) with `format=json&no_html=1&skip_disambig=1`.
  Returns a `SearchResult` (query, answer, abstract, abstract_url, sources);
  `grounding_text()` renders a facts snippet for the model (empty when nothing real).
  HTTP errors surface as `httpx.HTTPError` so the handler can apologize honestly.
- New `gateway/ember_gateway/weather.py`: `WeatherClient` calls the Open-Meteo
  forecast API; `Weather.describe()` is entirely deterministic (condition from WMO
  code, temperature, apparent temp, wind km/h, place). Config via `EMBER_WEATHER_LAT`,
  `EMBER_WEATHER_LON`, `EMBER_WEATHER_PLACE`; lat/lon 0,0 disables the handler.
- New `gateway/ember_gateway/timers.py`: `TimerStore` is in-memory, per-device,
  bounded (`max_timers=12`). `add`, `list`, `cancel`, `due`. Elapsed timers are
  removed; the gateway announces them on the device's next voice turn (the Fire is
  push-to-talk, so the gateway cannot spontaneously push an alarm without polling).
- `commands.py` intents: `_match_search` (search/look up/google/find out/check what/
  look into), `_match_weather` (weather/forecast/temperature/rain/snow/sun/cloudy/
  hot/cold/what's it like outside), `_match_timer` durations incl. number words
  ("five minutes"), labels ("to stretch"), plus `timer-list` ("what timers are
  active") and `timer-cancel`. Search precedes weather so "look up the weather" is
  a search. `_match_timer` now matches plural "timers".
- `main.py`: module-level `search`, `weather`, `timers` singletons; handlers
  `handle_search`, `handle_weather`, `handle_timer_schedule/list/cancel`;
  due-timer announcement prepended to the reply text before synthesis.
- Gateway version bumped to `0.5.0`. Commit `3044b96` on `oc-updates`.
- Tests: 53 pass (`test_tools.py` added: WebSearchClient parsing/grounding/HTTP
  error, Weather.describe, TimerStore lifecycle/isolation/bounds; `test_commands.py`
  expanded with search/look-up/timer/timer-list/timer-cancel/weather intents).
- Docs updated: TODO.md (time/timers checked off, weather + web search done, next
  slice = calculations/unit conversions), PROJECT_PROGRESS.md (slice 3 note),
  gateway/README.md (command table rows), `.env.example` (weather vars),
  this log.
- REMINDER: the running Pi stack still predates slices 1-3; update with
  `sudo ./scripts/update-pi.sh` before live testing these tools.
- Next slice: deterministic calculations and unit conversions in the gateway.

## 2026-08-08 (continued) — Priority 3 slice 4: calculations and unit conversions

- New `gateway/ember_gateway/calc.py`, entirely deterministic and never routed to
  the language model:
  - `evaluate_arithmetic`: parses a numeric-only expression with `ast.parse(mode="eval")`
    and evaluates through a whitelisted walk (`_evaluate_ast`): `Expression`,
    `Constant`, `BinOp` (`+ - * / **`), and signed literals only. No names, calls,
    attributes, or imports are ever evaluated, so transcript text cannot execute
    code. `CalcError` normalizes syntax errors, division by zero, `OverflowError`,
    and non-finite results into a spoken apology.
  - `_translate_expression`: rewrites spoken words into symbols and number words
    into digits ("what is six times eight" -> `6*8`, "two to the power of ten" ->
    `2**10`, "10 divided by 4" -> `10/4`), then strips anything that is not a
    digit or operator; returns `None` when no digits remain (falls through to the
    model). Division by zero yields an honest "cannot work that out" instead of a
    hallucinated value.
  - `convert` + `_UNITS`/`_TO_BASE`: linear length, mass, volume, and speed units
    (metric + imperial) normalized through base-unit factors, so both directions
    work; `_find_category` rejects mixed-category requests (`CalcError`).
    `convert_temperature` handles Celsius/Fahrenheit/Kelvin with shifted scales.
  - `parse_calculation` tries arithmetic then conversion and returns the spoken
    answer or `None`.
- `commands.py`: `_match_calc` (action="calc") wired between weather and status.
  It deliberately runs on the RAW text (not the space-normalized form) so literal
  symbols like `2+2` survive; word forms work through normalization regardless.
  New intents: "what is 6 times 8?", "convert 10 kilometers to miles",
  "100 celsius in fahrenheit", "what is two plus two?".
- Gateway version bumped to `0.6.0`.
- Tests: 73 pass (`test_calc.py` added: 18 tests covering arithmetic/words/power/
  precedence/parentheses, unit conversion incl. miles/yards, temperature, and
  safe rejection of non-numeric input; `test_commands.py` +2 intent tests).
- Docs updated: TODO.md (calc bullet checked off), PROJECT_PROGRESS.md (slice 4
  note), gateway/README.md (command table rows), this log.
- REMINDER: update the Pi (`sudo ./scripts/update-pi.sh`) before live-testing
  slices 1-4 together.
- Next slice options: pause/resume timers, persistent reminders, or live
  verification of the cumulative Priority 3 gateway commands on the Pi/Fire.

## 2026-08-08 (continued) — Priority 3 slice 5: named-place + IP-geolocated weather

- `weather.py` upgraded beyond fixed lat/lon:
  - `Location` dataclass (lat/lon/place).
  - Keyless Open-Meteo **geocoding** (`geocoding-api.open-meteo.com/v1/search`):
    "what's the weather in Tokyo?" resolves any place by name.
  - **IP geolocation** fallback chain: `ipapi.co/json` → `ipwho.is` → `ipinfo.io`
    → `ip-api.com/json` (HTTP-only), each with its own tolerant parser
    (`_parse_ip_api` for `lat`/`lon`, `_parse_ip_payload` dispatch). The first
    provider that returns a valid location wins; HTTP/parse failures fall
    through to the next.
  - Resolution order: named place (always wins, geocoded, persisted) → stored
    location → IP detection → `EMBER_WEATHER_LAT/LON`. Result persisted to
    `EMBER_WEATHER_LOCATION_FILE` (default `/var/lib/ember/weather_location.json`),
    so detection happens once and is reused; a named place overwrites it.
  - `LocationUnavailable` yields an honest apology, never a hallucinated answer.
- `commands.py`: `_match_weather` now captures a trailing place ("in Paris") into
  `query`. `main.py`: `handle_weather(command.query or None)`.
- `config.py`: added `weather_location_file` (`EMBER_WEATHER_LOCATION_FILE`);
  `.env.example` documents the vars (defaults `0,0` = unset fallback).
- Version bumped to `0.7.0`; 81 tests passing (new geocode, stored-location,
  ip-parse, and place-capture tests).
- Live (unmocked) verification from the dev machine: IP detection resolved to
  Cape Town, Western Cape, South Africa; named lookups for Cape Town (light
  drizzle, 13C) and Tokyo (mainly clear, 24C, feels like 30) both correct.
- Deployed to the Pi via `sudo scripts/update-pi.sh`; `/health` OK, version 0.7.0.
- Remaining: user confirmation of the detected location, and a live Fire turn
  exercising "what's the weather" end to end.
