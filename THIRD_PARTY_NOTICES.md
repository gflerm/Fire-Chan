# Third-Party Notices

Fire-chan's original source code and documentation are licensed under the
Apache License 2.0. Dependencies, tools, frameworks, models, and voice assets
are not relicensed by Fire-chan and remain subject to their own terms.

This inventory covers the project's principal direct dependencies. Transitive
dependencies installed by PlatformIO, pip, the operating system, or external
installers may carry additional notices. Distributors of source or firmware
binaries are responsible for reviewing and satisfying the applicable licenses.

## Firmware dependencies

| Component | License | Upstream |
|---|---|---|
| M5Unified | MIT | <https://github.com/m5stack/M5Unified> |
| M5GFX | MIT; bundled fonts/assets may carry additional notices | <https://github.com/m5stack/M5GFX> |
| Adafruit NeoPixel | LGPL-3.0 | <https://github.com/adafruit/Adafruit_NeoPixel> |
| ArduinoHttpClient | Apache-2.0; its URL parser carries separate upstream notices | <https://github.com/arduino-libraries/ArduinoHttpClient> |
| ArduinoJson | MIT | <https://github.com/bblanchon/ArduinoJson> |
| Arduino ESP32 framework and Espressif toolchain | Multiple upstream licenses | <https://github.com/espressif/arduino-esp32> |

Adafruit NeoPixel's LGPL terms are especially relevant when distributing a
compiled firmware image. Retain its copyright and license notices and comply
with the LGPL requirements applicable to the form of distribution.

## Raspberry Pi gateway dependencies

The gateway declares FastAPI, HTTPX, python-multipart, tzdata, and Uvicorn.
Pip resolves these packages and their transitive dependencies at installation
time; their installed metadata and distributions contain the controlling
license texts.

The installer also obtains these external runtime components:

| Component | License or terms | Upstream |
|---|---|---|
| whisper.cpp | MIT | <https://github.com/ggml-org/whisper.cpp> |
| OpenAI Whisper `base.en` model weights | MIT | <https://github.com/openai/whisper> |
| Ollama | MIT for the referenced open-source repository | <https://github.com/ollama/ollama> |
| Llama 3.2 model | Meta Llama 3.2 Community License | <https://github.com/meta-llama/llama-models> |
| Piper (`piper-tts`) | GPL-3.0-or-later | <https://github.com/OHF-Voice/piper1-gpl> |
| Piper `en_GB-alba-medium` voice | Model repository terms; training dataset identified as CC BY 4.0 in its model card | <https://huggingface.co/rhasspy/piper-voices/blob/main/en/en_GB/alba/medium/MODEL_CARD> |

The models and runtime components are downloaded onto the user's Raspberry Pi;
they are not stored in this Git repository. Anyone redistributing a prepared Pi
image, model bundle, container, or appliance must include the license materials
required by those components and models.

## Trademarks

M5Stack, Arduino, Adafruit, Raspberry Pi, Whisper, Ollama, Llama, Piper, and
other names may be trademarks of their respective owners. Their appearance here
identifies compatible or external components and does not imply endorsement.
