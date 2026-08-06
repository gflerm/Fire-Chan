# Fire-chan Architecture Decision Log

## ADR-001: Use PlatformIO

### Status

Accepted — 2026-08-04

### Decision

Use PlatformIO with the Arduino framework and the `m5stack-fire` board profile.

### Consequences

- Builds, dependency resolution, upload, and serial settings are reproducible.
- Hardware-specific behavior remains isolated behind project modules.

## ADR-002: Keep the core experience independent of PSRAM

### Status

Accepted — 2026-08-04

### Context

The target Fire v2.5 repeatedly fails the ESP32 boot-time PSRAM memory test before
application startup. Display, input, audio, RGB, SD, and Wi-Fi diagnostics pass.

### Decision

Treat PSRAM as an optional accelerator. Keep the primary face canvas in internal
RAM and require every core local feature to boot and run when PSRAM is absent.

### Consequences

- The current hardware remains a valid development target.
- Large decoded assets, audio buffers, and future voice workloads must use bounded
  buffers, streaming, or optional feature checks.
- The hardware/configuration fault remains open for separate investigation.

## ADR-003: Split configuration between NVS and microSD

### Status

Accepted — 2026-08-04

### Decision

Store validated essential runtime preferences in ESP32 NVS. Write a human-readable
backup to `/config/device.json` on microSD when the card is available. Boot from
safe compiled defaults when persistent storage cannot be read.

### Consequences

- Fire-chan remains functional without a microSD card.
- Mute and demo-mode changes survive restart without blocking the animation loop.
- The SD layout matches the design specification and can later expand to sound,
  face, personality, web, cache, and log assets.

## ADR-004: Run Ember's voice intelligence on a local Raspberry Pi

### Status

Accepted — 2026-08-04

### Context

The Fire's ESP32 can capture and play streamed audio, but it cannot run useful
speech recognition, language, and neural speech models itself. The available host
is a Raspberry Pi 5 with 8 GB RAM and SSD storage. The project should begin without
cloud credentials or usage fees.

### Decision

Run the voice stack on the Pi: whisper.cpp `base.en` for speech recognition,
Ollama `llama3.2:3b` for conversation, and Piper `en_GB-alba-medium` as Ember's
initial voice. Put a small authenticated gateway in front of those services and
keep the model services accessible only from localhost.

### Consequences

- Voice processing stays on the local network and has no per-request provider fee.
- The Pi SSD stores models and temporary response audio; the Fire only needs bounded
  streaming buffers and does not require PSRAM for the voice pipeline.
- Response latency must be measured on the real Pi before model sizes are finalized.
- The shared gateway token must be provisioned into the Fire's NVS, never source control.

## ADR-005: Apply assistant directives after spoken acknowledgement

### Status

Accepted — 2026-08-04

### Context

The Ember gateway returns both an emotional expression and an optional device action.
Applying mute before playback would silence Ember's own acknowledgement, while applying
an expression during playback would be hidden by the higher-priority Speaking state.

### Decision

Parse gateway hints in a dedicated assistant module. Apply emotional expressions and
device actions after response playback completes. Unmute is the sole early action so
its confirmation can be heard. Treat time and status as informational actions with no
device-state side effect.

### Consequences

- Spoken confirmations and face transitions occur in a predictable order.
- Sleep changes the persistent resting expression; wake restores Neutral and may add a
  temporary emotional reaction.
- Explicit mute/unmute operations are idempotent and persist through the existing
  configuration manager.
- Unknown hints fail safely without changing device state.

## ADR-006: Use push-to-talk before considering wake-word listening

### Status

Accepted — 2026-08-04

### Decision

Keep Button A push-to-talk as the default voice interaction. Evaluate wake-word support
only after Wi-Fi provisioning, multi-turn reliability, interruption handling, and clear
privacy controls are complete.

### Consequences

- Recording is intentional and visibly indicated by the Listening expression.
- CPU, memory, feedback, and accidental-activation risks stay bounded.
- Any future wake word must include an obvious listening indicator and a physical or
  persistent software disable control.

## ADR-007: License original Fire-chan work under Apache 2.0

### Status

Accepted — 2026-08-04

### Decision

License Fire-chan's original source code and documentation under the Apache License,
Version 2.0, with `gflerm` as the copyright holder. Include the canonical `LICENSE`,
an attribution `NOTICE`, and a third-party inventory at the repository root.

### Consequences

- Users may use, modify, and redistribute Fire-chan, including commercially, under
  Apache 2.0's conditions and explicit patent grant.
- Contributions submitted for inclusion are licensed under Apache 2.0 unless stated
  otherwise through a separate agreement.
- Third-party libraries, tools, models, and voice assets retain their own licenses.
- Distributors must review those terms, particularly LGPL-licensed firmware
  dependencies, GPL-licensed Piper, and separately licensed language models.

## ADR-008: Make Gemini an optional conversation provider

### Status

Accepted for live evaluation — 2026-08-06

### Context

The Pi-hosted Ollama model preserves privacy and offline operation but contributes a
large part of Ember's turn latency. Google AI Studio provides a Gemini API key for a
low-latency trial, while the existing local STT, commands, and TTS already work well.

### Decision

Keep whisper.cpp and Piper local. Add a modular Gemini conversation provider using
`gemini-3.5-flash-lite`, minimal thinking, short output, and the existing Ember system
prompt. Select the provider through `/etc/ember/ember.env`; never store the key in Git.
Retain Ollama as the default and automatic fallback. Return stage timings for measured
comparison rather than assuming the cloud path is faster.

### Consequences

- The Fire firmware and LAN gateway protocol remain unchanged.
- Normal local commands such as time, name, mute, and sleep never call Gemini.
- When Gemini is selected, transcript text leaves the local network; recorded audio does not.
- Free-tier availability and quotas are external constraints, and free-tier prompts may
  be used by Google to improve its products under the current service terms.
- Loss of internet access, an API error, or a quota failure falls back to Ollama.
- Ollama remains a complete offline path and a requirement for the initial evaluation.
