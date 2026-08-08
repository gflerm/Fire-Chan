# Fire-chan ESP32-P4 Variant — Port Plan

**Version:** 0.1 (planning only — no code written yet)
**Target board:** Waveshare ESP32-P4-WIFI6 (SKU 32021)
**Companion radio:** onboard ESP32-C6 (SDIO 3.0 to P4)
**Display:** MIPI-DSI 2-lane (5″/7″/8″/10.1″ — panel not fitted on the base board)
**Audio:** onboard analog mic; 8 Ω 2 W speaker on MX1.25 header
**Storage:** TF card (SDIO 3.0) + 32 MB on-board Nor Flash + 32 MB in-package PSRAM
**Current reference build:** firmware `0.12.0-alarm-ringing` on M5Stack Fire v2.5; gateway `0.7.2`

---

## Status: planning only

This document is a seed for the next hardware variant of Fire-chan. **No code
has been written or branched yet.** The current `oc-updates` branch and the
Fire-chan build are the production state. The P4 work is a port / partial
rewrite planned for when the board is in hand and the panel is attached.

## Recommendation: start a new branch or new project before any code lands

The P4 port is large enough that it should not happen on `oc-updates` or `main`.
Two viable options:

- **New branch in this repo**, e.g. `p4-port` or `variant/esp32-p4`. Keeps
  history, gateway, and MD docs in one place. Good if you want the Fire
  build to keep evolving in parallel.
- **New repository** (e.g. `fire-chan-p4`). Cleaner separation. Good if the
  P4 firmware will diverge substantially from the Fire build, or if the
  P4 firmware gets its own gateway / voice stack.

**Action deferred.** This is a note, not a decision — pick when you're ready to
start coding. Until then, this document is the only artifact.

## Why move to the P4

- **Streaming early-start playback becomes possible.** The Fire's 4 KB
  internal-RAM audio buffer is the reason both `0.12.0-stream-play` and
  `0.12.0-stream-3buf` failed (red error, then Speaking-state lockup). A 16–32
  KB I²S-fed PSRAM ring buffer on the P4 removes the constraint entirely.
- **More headroom for face animation and tool-calling.** Dual RISC-V 400 MHz
  cores, 32 MB PSRAM, hardware DMA, image-processing accelerator.
- **Modern connectivity.** Wi-Fi 6 + Bluetooth 5 via the C6 companion; USB
  OTG 2.0 HS; SDIO 3.0; MIPI-CSI/DSI; H.264 encoder.
- **The Fire's PSRAM test fails and its 4 KB internal RAM is the bottleneck
  for every audio and animation change.** The P4 removes that ceiling.

## What survives unchanged

- `gateway/ember_gateway/` — entire host-side Python stack. The gateway has
  no hardware dependency. The personality, the deterministic intents, the
  tools, the timer/alarm/weather/search/calc handlers, and the local intent
  grammar all carry over without changes.
- `src/app/EventBus.cpp`, `src/app/BehaviorEngine.cpp` — pure C++ priority
  engine, board-agnostic. The `Alarmed` / `Error` / etc. expression enum is
  the same; only the canvas backend changes.
- `src/audio/AudioFeedback.cpp` — tone-cue queue is fine; only the output
  call changes from `M5.Speaker.tone()` to an I²S DAC driver.
- `src/storage/ConfigManager.cpp` — NVS works the same on P4. The
  empty-string NVS fix you just shipped carries over verbatim.
- `src/assistant/AssistantDirectiveParser.cpp` — pure C++, unchanged.
- `src/assistant/VoiceGatewayClient.cpp` — `HttpClient` + `WiFiClient` work
  on the P4 through the C6 shim. The multipart upload path is identical.
- `src/alarm/AlarmManager.cpp` — NTP-synced `time(nullptr)`, sequenced
  buzzer pattern, and the stale-deadline check are all portable. Only the
  output call (`M5.Speaker.tone()`) changes.
- The face renderers in concept — `Expression.h`, `EyeRenderer.cpp`,
  `MouthRenderer.cpp`, `AnimationScheduler.cpp`. Only the canvas init
  changes.
- All MD docs in this repo, with light updates for the new board.

## What is removed (Fire-specific)

- `M5Unified` autodetect and the whole `src/hardware/RgbFeedback.cpp` (10
  SK6812 NeoPixels on GPIO15) — no RGB LEDs on the P4 board.
- `M5.Power` calls in `Application::buildDeviceStatus` — no AXP2101, no
  battery management. Battery / voltage facts disappear or get replaced
  with USB-current sense.
- The 4 KB internal-RAM `downloadBuffer_` everywhere — the whole reason
  for the port.
- `VoiceRecorder.cpp`'s `gpio34` ADC path — P4 has a different analog
  mic route, needs a new driver (I²S MEMS mic on the dev board, or a
  separate ADC).
- `secrets.h` in `include/` — replaced with NVS-stored credentials (this
  is already a Priority 1 TODO).

## What is added (P4-specific)

- A real **streaming audio ring buffer in PSRAM**, fed by the gateway
  chunked WAV and drained by an I²S DMA. This is the headline payoff of
  the port. Replaces `src/audio/ResponseAudioPlayer.cpp`'s
  download-then-play path. The 5–11 s download wait in `future-upgrade.md`
  collapses to a few hundred ms of "first chunk ready" latency.
- A **MIPI-DSI display driver** (LovyanGFX or `esp_lcd` panel driver) for
  the 5–10.1″ panel. `FaceEngine.cpp`'s sprite init swaps from M5Unified's
  canvas to the new panel driver. All renderer code stays.
- An **I²S DAC / onboard amp driver** for the MX1.25 speaker. Replaces
  `M5.Speaker` everywhere.
- An **ESP32-C6 SDIO co-processor bring-up**. The P4 has no radio of its
  own. Arduino-ESP32 v3.x's `WiFi.h` shim *should* talk to the C6
  transparently, but this needs to be verified on real hardware. If the
  shim doesn't work cleanly, a small C6 firmware (Wi-Fi station + SDIO
  slave) plus a P4-side SDIO master driver is a multi-week side quest.
- Optional: an **RTC** or **battery + power path** if you want the device
  to be portable rather than USB-tethered. The Fire had none of this; the
  P4 gives you the option.

## Port plan (sized to the actual gap)

Roughly **6–8 weeks** part-time to reach parity with `0.12.0-alarm-ringing`,
plus another **1–2 weeks** to land the streaming ring buffer.

1. **Board bring-up** (~1 day). PlatformIO env `esp32-p4-wifi6` (Espressif
   BSP), `board = esp32-p4-wifi6`, pin map from the schematic. `Serial.println`
   + a simple LGFX sprite showing "hello" on the 10.1″ DSI panel.
2. **Wi-Fi via the C6** (~2–3 days, with risk). Verify Arduino-ESP32 v3.x
   Wi-Fi works through the C6 shim. If not, port the C6 firmware to bring
   up the SDIO link.
3. **Audio path rewrite** (~3–4 days). I²S DAC driver, PSRAM ring buffer,
   chunked I2S-fed player. Replaces `ResponseAudioPlayer`. This is the
   big one and the only meaningful performance win on day one.
4. **Face engine port** (~2–3 days). Swap canvas init; keep all renderers.
5. **Voice pipeline port** (~2 days). I²S / onboard analog mic, replace
   `VoiceRecorder`'s ESP32 ADC path.
6. **Re-assemble the rest** (~1 week). Buttons (BOOT + RST on the board;
   free GPIOs for additional A/B/C equivalents), `AlarmManager`,
   `ConfigManager`, `VoiceGatewayClient`, `NetworkManager` (NTP sync
   already works the same way on the P4).
7. **Streaming early-start playback** (~1–2 weeks, the goal that justified
   the move). Wire the ring buffer so speech starts within ~300 ms of the
   first WAV chunk arriving.

## What to bring across from the gateway (no change required)

The gateway's TODOs (tool-calling, etc.) become *more* valuable on the P4
because the response audio arrives in ~300 ms instead of ~5 s. Tool-calling
turns into a real conversation instead of a 12-second pause. None of this
needs to change on the Pi side.

## Open questions before starting

- Which panel size do you want to standardise on? (5″ is plenty for a
  320×240-class face, 10.1″ gives room for a richer UI but the face
  proportions change.)
- Do you want a microphone-array / voice-activity-detection upgrade? The
  P4 has hardware VAD; the Fire did not.
- Do you want the C6 to host any logic (BLE provisioning, voice wake-word
  offload) or stay a pure radio bridge?
- Battery / portable use, or USB-tethered only?
- New repository or new branch in this repo? (Recommendation: decide
  before the first commit lands.)

## Pointers

- `future-upgrade.md` — the original case for moving to a P4-class board.
- `Fire-chan_Project_Brief_v0.1.md` — overall goal and scope (unchanged).
- `TODO.md` — the live priority list; the P4 work is not on it yet and
  should be added once you start.
- `gateway/README.md` and `gateway/ember_gateway/` — the gateway stack,
  unchanged by the port.