# Ember gateway for Raspberry Pi 5

This gateway keeps Ember's speech and conversation processing on the local network:

1. `whisper.cpp` (`base.en`) turns the Fire's WAV recording into text.
2. Built-in commands handle immediate device actions without a language model.
3. Ollama with `llama3.2:3b` produces short conversational replies.
4. Piper with the `en_GB-alba-medium` voice turns the reply into a WAV file.
5. The gateway returns the transcript, reply, expression/action hints, and audio URL.

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

## Verify

```bash
TOKEN="paste-the-generated-token"
curl -H "X-Ember-Token: ${TOKEN}" http://127.0.0.1:8088/health
```

Do not commit the generated token. The Fire will eventually store the Pi address and token
in its existing persistent configuration. Until that firmware link is added, a computer on
the same LAN can test a recorded prompt with:

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

Configuration is in `/etc/ember/ember.env`. The gateway only exposes port `8088`; the model
services listen on the Pi itself. Response WAV files older than one hour are automatically
removed.
