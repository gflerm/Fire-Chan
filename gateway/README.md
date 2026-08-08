# Ember gateway for Raspberry Pi 5

This gateway keeps Ember's speech and voice generation on the local network, with a
selectable local or cloud conversation provider:

1. `whisper.cpp` (`base.en`) turns the Fire's WAV recording into text.
2. Built-in commands handle immediate device actions without a language model.
3. Ollama `llama3.2:3b` or Gemini `gemini-3.5-flash-lite` produces short replies.
4. Piper with the `en_GB-alba-medium` voice turns the reply into a WAV file.
5. The gateway returns the transcript, reply, expression/action hints, and audio URL.

## Current deployed baseline

The stack is running on a Raspberry Pi 5 with 8 GB RAM and SSD at `192.168.8.107:8088`.
Fire firmware `0.11.0-download-conn` records a 16 kHz mono prompt, uploads it in a
background task, downloads the response WAV to microSD, streams it with three bounded
internal-RAM buffers, and applies the returned expression or supported device action.
The accepted Fire speech output level is 141. Ollama remains the default and requires
no cloud account. Gemini is optional and falls back to Ollama on network/API failure.
Replies are served as canonical 8 kHz WAVs (`EMBER_AUDIO_RATE_HZ=8000`) to cut response
bytes ~3.5x.

The Pi 5 should run 64-bit Raspberry Pi OS from its SSD and have network access during
installation. A wired Ethernet connection is recommended for the initial model downloads.

## Install on the Pi

Copy this `gateway` directory to the Pi, then run:

```bash
cd gateway
chmod +x scripts/install-pi.sh
sudo ./scripts/install-pi.sh
```

The installer creates a dedicated unprivileged `ember` account, installs the stack under
`/opt/ember`, stores generated audio under `/var/lib/ember`, and registers four services.
It prints the gateway address and a randomly generated shared token at the end.

For later gateway-only updates, copy the refreshed directory to the Pi and run:

```bash
cd gateway
chmod +x scripts/update-pi.sh
sudo ./scripts/update-pi.sh
```

## Enable optional Gemini conversation

Gemini only replaces the conversation stage; whisper.cpp and Piper continue to run
locally. The low-latency default is `gemini-3.5-flash-lite` with minimal thinking.

After updating the gateway, run this on the Pi:

```bash
chmod +x scripts/configure-gemini.sh
sudo ./scripts/configure-gemini.sh
```

Paste the Google AI Studio API key at the hidden prompt. The script stores it only in
`/etc/ember/ember.env` with `root:ember` ownership and mode `0640`, selects Gemini, keeps
Ollama as the automatic fallback, restarts the gateway, and prints the health result.
Never paste the key into source code, `.env.example`, chat, screenshots, or Git.

To return to fully local inference, edit `/etc/ember/ember.env`, set
`LLM_PROVIDER=ollama`, and restart `ember-gateway`.

The Gemini free tier has usage limits and, according to Google's current terms, free-tier
content may be used to improve its products. Prompts sent to Gemini leave the local
network; audio remains local because only the whisper.cpp transcript is submitted.

## Verify

```bash
TOKEN="paste-the-generated-token"
curl -H "X-Ember-Token: ${TOKEN}" http://127.0.0.1:8088/health
```

Do not commit the generated token. The current Fire firmware reads Wi-Fi and gateway
credentials from the ignored `include/secrets.h`; the planned provisioning work will move
them into NVS. A computer on the same LAN can test a recorded prompt with:

```bash
curl -H "X-Ember-Token: ${TOKEN}" \
  -F "file=@last_prompt.wav;type=audio/wav" \
  http://PI_ADDRESS:8088/v1/voice
```

## Service checks

```bash
sudo systemctl status ember-gateway whisper-ember piper-ember ollama
sudo journalctl -u ember-gateway -f
```

Configuration is in `/etc/ember/ember.env`. The gateway only exposes port `8088`; local
model services listen on the Pi itself. Response WAV files older than one hour are
automatically removed. Voice responses now include per-stage timing values for comparing
transcription, conversation, synthesis, and total gateway latency.

Time questions are answered directly from the Pi clock using `EMBER_TIMEZONE` rather than
being sent to the language model. The default is `Africa/Johannesburg`; verify the Pi clock
with `timedatectl` after installation.

## Supported local commands

| Prompt intent | Device result |
|---|---|
| Ask the time | Deterministic local time from `Africa/Johannesburg` |
| Ask Ember's name | Local identity response |
| Go to sleep | Spoken acknowledgement, then Sleeping state |
| Wake up | Audio restored if needed, then awake expression |
| Mute | Spoken acknowledgement, then persistent mute |
| Unmute | Persistent unmute before spoken acknowledgement |
| "What can you do?" | Help listing capabilities, no other side effect |
| "Say that again" / "repeat" | Last assistant reply replayed from device memory |
| "Set the volume to N" | Action `volume=N` for the device to clamp to 0–100 |
| "Turn it up / down" | Relative step action `volume=+10` / `volume=-10` |
| Status | Local online-status response |

Volume intents resolve deterministically in the gateway: absolute requests pass the
target level and relative ones pass a fixed ±10 step, so the Fire owns the current level
and clamps the result. Device-side application of `volume=` actions and status facts
(Wi-Fi, battery, mute, storage, firmware) is tracked in the root `TODO.md` as
Priority Three slice two.

Multi-turn memory, timers, reminders, provisioning, and assistant interruption behavior
are tracked in the root `TODO.md`.
