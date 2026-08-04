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
