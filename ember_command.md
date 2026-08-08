# Ember Command Reference

What you can say to Fire-chan and how Ember responds. All local commands resolve
deterministically on the Raspberry Pi gateway **before** the language model is
consulted, so answers stay grounded and never hallucinate. Anything not listed
here falls through to the conversation model.

Runnable intent matchers live in `gateway/ember_gateway/commands.py`; tools that
back the information commands live in `gateway/ember_gateway/` (`search.py`,
`weather.py`, `timers.py`, `calc.py`).

## Time and date

| Say | Ember's result |
|---|---|
| "What time is it?" / "Tell me the time" | Local time from the Pi clock (`EMBER_TIMEZONE`) |
| "What is the date?" / "What day is it?" | Today's weekday, day, month, and year |
| "What's today's date?" | Same as above |

Grounded in the Pi clock/timezone — never guessed by the model.

## Conversation control

| Say | Ember's result |
|---|---|
| "What can you do?" / "Help me" | Lists her capabilities |
| "Say that again" / "Repeat" | Replays the last reply from per-device memory |
| "Forget our conversation" / "Start over" / "Clear your memory" | Wipes the session history |
| "What is your name?" / "Who are you?" | "I'm Ember. It's lovely to meet you." |

Multi-turn history is per-device on the Pi and expires after a configurable idle
period (`SESSION_IDLE_SECONDS`).

## Device control

| Say | Ember's result |
|---|---|
| "Go to sleep" / "Good night" | Sleeping expression after the reply |
| "Wake up" / "Good morning" | Awake expression |
| "Mute" / "Be quiet" | Spoken ack, then persistent mute |
| "Unmute" / "You can speak" | Unmutes before the spoken confirmation |
| "Set the volume to 40" | Action `volume=40`; the Fire clamps and persists 0–100 |
| "Set the volume to 40%" / "...to 40 percent" | Same absolute target |
| "Turn it up" / "Louder" | Relative step `volume=+10` |
| "Turn it down" / "Quieter" / "Decrease" | Relative step `volume=-10` |

Volume changes survive restart (persisted to NVS on the Fire).

## Timers and reminders

| Say | Ember's result |
|---|---|
| "Set a timer for 5 minutes" | 5-minute timer on this device |
| "Remind me in 30 seconds to stretch" | Named timer "stretch" |
| "Set a timer for two hours" | Number words understood |
| "What timers are active?" | Lists pending timers |
| "Cancel my timers" / "Cancel the timer" | Removes this device's timers |

Timers are in-memory per-device on the Pi (max 12). When one elapses, Ember
announces it on the device's **next** push-to-talk turn (the Fire cannot receive
spontaneous push alerts yet). Cancellation currently removes all of the device's
timers; per-label cancel is a planned refinement.

## Alarms

| Say | Ember's result |
|---|---|
| "Set an alarm for 5:30 AM" | Fire fires a 3-second alarm sound at that time |
| "Set an alarm for 7:00 PM" | Same, evening |
| "Set an alarm in 10 minutes" | Relative alarm |
| "What alarms are set?" | Lists pending alarms |
| "Dismiss my alarms" / "Cancel the alarm" | Removes this device's alarms |

When an alarm is set, the gateway hands the absolute Unix deadline to the
Fire, which schedules a local NTP-synced alarm. At the deadline the Fire plays
a distinctive 3-second beeping pattern, shows the Alarmed expression, and
keeps the alarm up until the user dismisses it (press Button B, or hold
Button A to talk). Alarms survive restart (NVS) and Wi-Fi loss.

## Information and utilities

### Web search

| Say | Ember's result |
|---|---|
| "Search the web for how to make sourdough" | Grounded answer from DuckDuckGo |
| "Look up the weather in London" | Search (not local weather) |
| "Google X" / "Find out about X" | Same grounded search |
| "Check what the capital of Japan is" | Same |

Results are fed to the provider as grounding; on failure Ember apologizes
honestly instead of guessing.

### Weather

| Say | Ember's result |
|---|---|
| "What's the weather like?" | Current conditions, temperature, wind for the gateway's location |
| "What's the weather in Paris?" | Conditions for that place, geocoded by name anywhere in the world |
| "Is it raining?" / "How hot is it?" / "Forecast?" | Same weather answer |

Powered by Open-Meteo (no API key). A named place in the request always wins.
Without one, the gateway uses its stored location, otherwise IP-based geolocation
(tries `ipapi.co` → `ipwho.is` → `ipinfo.io` → `ip-api.com`), otherwise
`EMBER_WEATHER_LAT/LON`. The resolved location is persisted at
`EMBER_WEATHER_LOCATION_FILE` (`/var/lib/ember/weather_location.json`) so it's
detected once and reused.

### Calculations and conversions

| Say | Ember's result |
|---|---|
| "What is 6 times 8?" | Compute, spoken words understood |
| "What is two plus two?" | Number words understood |
| "What is 10 to the power of 10?" | Powers / "squared" / "cubed" |
| "Convert 10 kilometers to miles" | Length conversion |
| "2 miles to yards" / "10 kg in pounds" | Same-category conversions |
| "100 celsius in fahrenheit" | Temperature conversion (C/F/K) |

Safer: no arbitrary code is ever executed (AST-whitelisted arithmetic). Unclear
math falls through to the model.

### Device status

| Say | Ember's result |
|---|---|
| "How are you?" / "Status" | Online/offline and battery from the Fire's status header |
| "What's your battery?" | Battery percentage |
| "Are you connected to Wi-Fi?" | Wi-Fi state |
| "How much storage do you have?" | Free microSD space |
| "What firmware are you running?" | Firmware version |

Answers come from the `X-Ember-Device-Status` facts the Fire uploads with every
turn; unknown values are omitted rather than invented.

## What is NOT a local command

Anything else — story prompts, opinions, open questions, or malformed math — is
sent to the language model (Ollama by default, optional Gemini) and answered in
Ember's voice with its current conversation memory.