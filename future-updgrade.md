# Future Hardware Upgrade: ESP32-P4 and Jetson Nano

_Recorded: 6 August 2026_

## Summary

The ESP32-P4 is a considerably better platform for the next Fire-Chan/Ember hardware revision. The original Jetson Nano, however, is unlikely to improve Ollama performance and may be slower or harder to maintain than the Raspberry Pi 5.

## ESP32-P4: Worthwhile Upgrade

The proposed ESP32-P4 board provides:

- Dual 400 MHz RISC-V cores versus the original ESP32's 240 MHz cores
- 32 MB PSRAM for large audio, camera, display, and network buffers
- Hardware DMA, image processing, JPEG/H.264, camera, and display interfaces
- High-speed USB, Ethernet, SD/MMC, multiple SPI and I2S controllers
- Voice-activity-detection hardware

These capabilities make it particularly suitable for an interactive voice and display device. [Espressif's specifications](https://www.espressif.com/en/products/socs/esp32-p4) position the P4 for rich interfaces, cameras, displays, and smart voice terminals.

The largest benefit is not simply faster processing. The additional RAM enables a redesigned audio path:

```text
Pi synthesizes speech
        |
        v
Audio arrives at the P4 in chunks
        |
        v
P4 stores chunks in a PSRAM ring buffer
        |
        v
Speaker starts immediately while downloading continues
```

Currently, Ember waits for the complete WAV file before playback. Measurements from the existing M5Stack Fire implementation showed approximately:

- Pi transcription, Gemini, and TTS: 4.1-4.3 seconds
- Fire audio download: 5-11 seconds
- Total time before ready/playback: 13-20 seconds

Streaming into PSRAM and playing through I2S as data arrives could remove much of the 5-11-second download wait. Merely copying the existing firmware onto the P4 without changing the audio architecture would provide a smaller improvement.

The 32 MB PSRAM also removes the practical memory limitation encountered on the M5Stack Fire and provides room for:

- Streaming audio ring buffers
- Double-buffered graphics
- Camera frame buffers
- Larger network transfers
- Smoother animations while audio and networking are active

The P4 still is not suitable for running Ember's general-purpose LLM locally. Small wake-word, voice-activity-detection, and image-processing models are realistic; Gemini or a capable conversational model should remain on the Pi or a hosted service.

## SPI Display Considerations

An SPI display will work, with the following preferred arrangement:

- Put the display on its own SPI controller where possible.
- Use SD/MMC, rather than shared SPI, for storage.
- Use DMA for display transfers.
- Keep audio on a dedicated I2S interface.
- Connect the camera through MIPI-CSI or the board's camera interface rather than SPI.

The P4 supports MIPI camera and display interfaces, parallel display interfaces, and multiple SPI and I2S controllers. See the [ESP32-P4 datasheet](https://documentation.espressif.com/esp32-p4_datasheet_en.html).

One important point is that the ESP32-P4 itself does not contain Wi-Fi. P4 development boards commonly include an ESP32-C6 companion chip, but this must be confirmed for the selected board. Espressif describes using an ESP32-C/S companion for wireless connectivity. If the board exposes Ethernet, wired Ethernet could provide very consistent, low-latency communication.

## Jetson Nano and Ollama

Moving Ollama from the Raspberry Pi 5 to the original Jetson Nano is not recommended.

Although the Nano has 128 CUDA cores, it uses an older Maxwell GPU with only 4 GB of shared memory. NVIDIA lists its quad-core Cortex-A57, Maxwell GPU, and 4 GB LPDDR4 specifications on the [Jetson Nano product page](https://developer.nvidia.com/embedded/jetson-nano).

The more serious issue is software support:

- Original Nano hardware remains on the JetPack 4 generation.
- JetPack 4.6 uses Ubuntu 18.04 and CUDA 10.2. See [NVIDIA's JetPack 4.6 details](https://developer.nvidia.com/embedded/jetpack-sdk-46).
- Current Ollama Docker documentation provides Jetson selections for JetPack 5 and 6, not JetPack 4. See the [Ollama Docker documentation](https://docs.ollama.com/docker).
- Ollama maintainers concluded that supporting JetPack 4 was problematic because its CUDA and compiler requirements conflict. Users also reported Ollama falling back to CPU. See the [Ollama JetPack 4 discussion](https://github.com/ollama/ollama/issues/4140).

Ollama's ARM64 package may install, but that does not guarantee GPU acceleration. On CPU alone, the older Cortex-A57 Nano will probably perform worse than the Pi 5. A specially compiled `llama.cpp` build might use the Nano GPU, but with 4 GB RAM and an old CUDA stack it would be an experimental side project rather than a dependable Ember backend.

The Jetson Nano would not accelerate the current Gemini route because Ollama is presently only the automatic fallback.

## Recommended Architecture

```text
ESP32-P4
  Mic, speaker, display, and optional camera
  Streaming audio playback and animations
             |
             | Ethernet or C6 Wi-Fi companion
             v
Raspberry Pi 5
  Whisper STT
  Piper TTS
  Tools, memory, and gateway
  Gemini primary / Ollama fallback

Jetson Nano
  Optional camera or experimental vision processing
  Not the primary Ollama server
```

## Recommendation

1. Adopt the ESP32-P4 as Ember's next main controller.
2. Keep the Raspberry Pi 5 as the voice and AI gateway.
3. Keep Gemini as the primary inference provider and Ollama on the Pi as the fallback.
4. Implement streaming audio directly into a PSRAM ring buffer.
5. Use the Jetson Nano later for optional computer-vision experiments.
6. Before starting the port, confirm the exact P4 board model, Wi-Fi companion, camera interface, amplifier and microphone chips, display pins, and PSRAM configuration.
