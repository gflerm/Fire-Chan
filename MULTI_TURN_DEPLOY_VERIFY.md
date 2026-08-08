# Multi-turn conversation — deploy and verify (Priority 2, scope A)

Scope A: Fire-chan keeps bounded, anonymous per-device conversation history on the
Pi gateway so follow-ups resolve, with idle expiry and an explicit "forget" command.
Firmware sends a stable device id (`X-Ember-Device`); the gateway retains the thread.

## What changed

| Side | Version | Change |
|---|---|---|
| Firmware | `0.10.0-device-id` | Stable anonymous id, generated once, in NVS + SD backup; sent on `/v1/voice` |
| Gateway | `0.3.0` | Per-device session memory, idle expiry, forget command, grounded date command |

## 1. Deploy the gateway on the Pi

On the Pi, from a copy of this repo's `gateway` directory:

```bash
cd gateway
chmod +x scripts/update-pi.sh
sudo ./scripts/update-pi.sh
```

`update-pi.sh` stops `ember-gateway`, copies the new package under `/opt/ember`,
installs requirements, ensures `EMBER_TIMEZONE` exists, restarts the service, and
prints the health result. It does not overwrite `/etc/ember/ember.env`, so the token
and any `LLM_PROVIDER=`/Gemini setting are preserved.

Optional new tuning knobs (add to `/etc/ember/ember.env`, then restart the service):

```bash
SESSION_MAX_TURNS=6          # recent turns kept per device
SESSION_IDLE_SECONDS=1800    # gap that starts a fresh session
```

```bash
sudo systemctl restart ember-gateway
```

The firmware is already uploaded on the Fire; no firmware change is needed for this
deploy.

> Note: history is in-memory. A gateway restart clears it by design, matching the
> "an expired session does not restore old conversation" acceptance criterion.

## 2. Verify the gateway is serving the device header

Obtain the token from `/etc/ember/ember.env`, then check health:

```bash
TOKEN="$(sed -n 's/^EMBER_TOKEN=//p' /etc/ember/ember.env)"
curl -sS -H "X-Ember-Token: ${TOKEN}" http://127.0.0.1:8088/health
```

Live end-to-end test from a machine on the LAN (replace `PI_ADDR`):

```bash
curl -sS -H "X-Ember-Token: ${TOKEN}" -H "X-Ember-Device: test-1234" \
  -F "file=@last_prompt.wav;type=audio/wav" \
  http://PI_ADDR:8088/v1/voice
```

Repeating the same command with the same `X-Ember-Device` exercises the stored
history; a different device id proves sessions are isolated.

## 3. Verification matrix (on hardware)

| # | Prompt | Expect |
|---|---|---|
| 1 | "What is my name?" (with device, after setup) | Ember answers and "My name is Ember." only after an accepted identity; record the reply |
| 2 | "What did I just ask you?" or a "tell me more" follow-up | Reply references the previous turn (history working) |
| 3 | "What is the date?" / "What day is it?" | Deterministic local date/day, not a model guess |
| 4 | "Forget our conversation" / "Start over" | Acknowledges forgetting; the next follow-up no longer refers to prior turns |
| 5 | Five related turns in a row | Every turn resolves from the accumulated context |
| 6 | Change the device header (or a second unit) | Separate, isolated history (no cross-talk) |
| 7 | Wait longer than `SESSION_IDLE_SECONDS`, then ask a follow-up | Fresh session; no memory of the earlier thread |
| 8 | Restart `ember-gateway` mid-session | History cleared by design; no old conversation resurfaces |

## 4. Guards to re-check

- Device id is random and anonymous — never the MAC or chip serial.
- Device id and the shared token are not serial-logged, SD-backed, or in Git.
  (The device id does appear in `/config/device.json` by design; it is anonymous.)
- Local commands (time, date, name, sleep, wake, mute, forget) never call the LLM,
  so they stay deterministic regardless of any history.

## 5. On completion

Record results and the measured five-turn behavior in `PROJECT_PROGRESS.md` and tick
the Priority 2 scope A items in `TODO.md`. Deferred scope B (streamed playback,
request-overhead reduction, 16 kHz/codec, ten-prompt comparison) is a separate
milestone and is not part of this deploy.

Status: scope A implemented (device id `0.10.0-device-id`, gateway memory `0.3.0`);
live five-turn on-device verification remains pending user execution.