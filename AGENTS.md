# AGENTS.md — Fire-chan

Project-specific guidance for OpenCode and other agents working on this repo.

## Feature reference (gateway tools)

Deterministic, unit-tested gateway capabilities live in `gateway/ember_gateway/`.
Each tool has a client module, intent matchers in `commands.py`, handler functions
in `main.py`, and tests in `gateway/tests/`. Add a new tool by following this
pattern, then bump `__version__` in `gateway/ember_gateway/__init__.py`.

| Capability | Router module | Intent (commands) | Handler (main) | Tests |
|---|---|---|---|---|
| Grounded web search | `search.py` (`WebSearchClient`) | `_match_search` | `handle_search` | `test_tools.py` |
| Weather | `weather.py` (`WeatherClient`) | `_match_weather` (+ trailing-place capture) | `handle_weather` | `test_tools.py` |
| Timers / reminders | `timers.py` (`TimerStore`) | `_match_timer` (+`timer-list`/`timer-cancel`) | `handle_timer_schedule` / `handle_timer_list` / `handle_timer_cancel` | `test_tools.py` |
| Alarms | `timers.py` (`TimerStore`, `kind="alarm"`) + firmware `AlarmManager` | `_match_alarm` (+`alarm-list`/`alarm-dismiss`) | `handle_alarm_schedule` / `handle_alarm_list` / `handle_alarm_dismiss` (returns `alarm_time` in response) | `test_commands.py` |
| Calculations / unit conversions | `calc.py` (AST-safe `evaluate_arithmetic`, `parse_calculation`) | `_match_calc` (raw text) | none (reply resolved at match time) | `test_calc.py` |
| Device status | — (facts in `commands.py`) | `parse_device_status` / `_status_describe` | status branch | `test_commands.py` |
| Time / date / help / repeat / volume | `commands.py` | `match_local_command` | repeat branch | `test_commands.py` |

### Conventions

- Local commands resolve deterministically before the language model; they must
  never depend on the LLM and must remain grounded (time from the Pi clock, status
  from the `X-Ember-Device-Status` header, search/weather from live APIs).
- Create/list setting: tool modules are injected at module import in `main.py`
  (`search = WebSearchClient()`, `weather = WeatherClient(...)`,
  `timers = TimerStore()`).
- A tool that needs configuration must read it through `Settings` env vars
  (e.g. `EMBER_WEATHER_LAT`); document any new vars in `gateway/.env.example`.
- Timers/reminders are in-memory per-device on the Pi; due-timer announcements
  are prepended to the next voice reply (the Fire is push-to-talk and cannot
  receive spontaneous push alerts yet).
- Fallbacks must be honest: on API/HTTP failure return a clear apology, never
  hallucinate facts (`httpx.HTTPError` is caught in the handler).
- Handlers return `(reply, expression, action)` tuples; the gateway reply is
  deterministic for commands.

## Development workflow

- Tests: `PYTHONPATH=gateway python -m unittest discover -s gateway/tests`
  (88 tests currently). Run them after any gateway change.
- Version bumps: `gateway/ember_gateway/__init__.py` — keep it in the same commit
  as the change.
- Docs: update `TODO.md` (check off the item), `PROJECT_PROGRESS.md` (status
  note), `gateway/README.md` (command table), and `opencode_update.md` (session
  log) whenever a milestone/slice completes. Do NOT name AGENTS.md in those docs.
- After gateway changes, the Pi must be updated with
  `sudo ./scripts/update-pi.sh` on the Pi (token/provider config in
  `/etc/ember/ember.env` is preserved). The running Pi stack often lags the repo.
- Firmware work: PlatformIO `firechan` env, `pio run` / `pio run --target upload
  --upload-port COMxx` / `pio device monitor --port COMxx --baud 115200`. Never
  require PSRAM (the target Fire fails its PSRAM test). Keep streaming playback
  experiments behind a serial capture before re-flashing.
- The commit/rollback pattern for risky firmware experiments is documented in
  `opencode_update.md` (0.12.0-stream attempts). Known-good = `0.11.0-download-conn`.