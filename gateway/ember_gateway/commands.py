from dataclasses import dataclass
from datetime import datetime
import re
from zoneinfo import ZoneInfo


@dataclass(frozen=True)
class CommandResult:
    reply: str
    expression: str = "happy"
    action: str | None = None
    clear_session: bool = False
    repeat_last: bool = False


VOLUME_HELP = (
    "I can tell the time and date, sleep and wake, mute and unmute, "
    "report my status, adjust the volume, repeat the last thing I said, "
    "and start a new conversation when you ask."
)


def _volume_target(step: int) -> str:
    """Encode a relative volume change as a device-appliable action hint.

    The Fire owns the current level and its 0-100 safety limits, so the gateway
    sends the requested step and the device clamps and persists the result.
    """
    return f"volume={'+' if step >= 0 else ''}{step}"


def parse_device_status(text: str) -> dict[str, str]:
    """Parse a compact ``key=value;...`` device-status header.

    The Fire sends its locally held facts (firmware, Wi-Fi, battery, free
    storage, mute) on every voice request. Unknown or invalid fields are
    simply dropped so a missing feature cannot break status answers.
    """
    facts: dict[str, str] = {}
    for pair in (text or "").split(";"):
        if "=" not in pair:
            continue
        key, value = pair.split("=", 1)
        key = key.strip().lower()
        value = value.strip()
        if key and value:
            facts[key] = value
    return facts


def _status_describe(facts: dict[str, str]) -> str:
    parts = []
    if "wifi" in facts:
        parts.append("connected to Wi-Fi" if facts["wifi"] == "1" else "offline")
    if "battery" in facts:
        try:
            percent = int(facts["battery"])
            if percent >= 0:
                parts.append(f"about {percent}% battery")
        except (KeyError, ValueError):
            pass
    if "sd_free_mb" in facts:
        try:
            free_mb = int(facts["sd_free_mb"])
            if free_mb >= 0:
                parts.append(f"about {free_mb} megabytes free on storage")
        except ValueError:
            pass
    if "fw" in facts:
        parts.append(f"running firmware {facts['fw']}")
    return ", ".join(parts) or None


def match_local_command(
    text: str,
    timezone: str = "Africa/Johannesburg",
    now: datetime | None = None,
    device_status: str = "",
) -> CommandResult | None:
    normalized = re.sub(r"[^a-z0-9']+", " ", text.lower()).strip()

    if re.search(r"\b(what('| i)s the time|what time is it|tell me the time|current time)\b", normalized):
        current = now or datetime.now(ZoneInfo(timezone))
        hour = current.strftime("%I").lstrip("0") or "0"
        return CommandResult(
            f"It's {hour}:{current:%M} {current:%p}.",
            "happy",
            "time",
        )
    if re.search(r"\b(what('| i)s the date|what date is it|today('| i)?s date|what day is it)\b", normalized):
        current = now or datetime.now(ZoneInfo(timezone))
        return CommandResult(
            f"Today is {current:%A}, {current:%d} {current:%B} {current:%Y}.",
            "happy",
            "time",
        )
    if re.search(r"\b(forget|clear|wipe|reset)\b.*\b(conversation|memory|everything|this|it)\b", normalized) or re.search(r"\bstart over\b", normalized):
        return CommandResult(
            "I've forgotten our conversation. What would you like to talk about?",
            "neutral",
            clear_session=True,
        )
    if re.search(r"\b(what('| i)s your name|who are you)\b", normalized):
        return CommandResult("I'm Ember. It's lovely to meet you.", "happy")
    if re.search(r"\b(what can you do|what are you able to do|need some help|help me|how do you work)\b", normalized):
        return CommandResult(VOLUME_HELP, "happy", "help")
    if re.search(r"\b(repeat|say that again|say it again|run that by me once more)\b", normalized):
        return CommandResult("I'll repeat my last reply.", "neutral", repeat_last=True)
    if re.search(r"\b(go to sleep|sleep now|good ?night)\b", normalized):
        return CommandResult("Good night. I'll be right here when you need me.", "sleepy", "sleep")
    if re.search(r"\b(wake up|good morning)\b", normalized):
        return CommandResult("I'm awake and ready.", "excited", "wake")
    if re.search(r"\b(mute|be quiet)\b", normalized):
        return CommandResult("Muted.", "neutral", "mute")
    if re.search(r"\b(unmute|you can speak)\b", normalized):
        return CommandResult("Voice is back on.", "happy", "unmute")
    if volume := _match_volume(normalized):
        return volume
    if re.search(
        r"\b(status|how are you|battery|wi-?fi|wifi|storage|space|firmware|"
        r"what are your levels|how much storage|are you connected)\b",
        normalized,
    ):
        describe = _status_describe(parse_device_status(device_status))
        reply = f"I'm {describe}." if describe else "I'm online and feeling bright."
        return CommandResult(reply, "happy", "status")
    return None


def _match_volume(text: str) -> CommandResult | None:
    """Resolve volume intents into device-appliable steps.

    Absolute requests (\"set the volume to 40\") pass the target level; relative
    requests (\"turn it up\", \"quieter\") pass a fixed safe step. The Fire owns the
    current level and clamps the result to 0-100, so this stays deterministic.
    """
    # "set the volume to 40 (percent)" / "set volume to 40%" / "volume to 40"
    absolute = re.search(r"\bvolume\s*(?:to|at|is)?\s*(\d{1,3})\s*(?:percent|%)?\b", text)
    if absolute:
        target = max(0, min(int(absolute.group(1)), 100))
        return CommandResult(f"I'll set the volume to {target}.", "happy",
                             f"volume={target}")
    if re.search(
        r"\b(turn (?:it|the volume) (?:up|down)|volume (?:up|down)"
        r"|(?:make )?it (?:louder|quieter)|louder|quieter|increase|decrease)\b",
        text,
    ):
        step = -10 if re.search(r"\b(quieter|decrease|down)\b", text) else 10
        return CommandResult("I'll adjust the volume.", "happy", _volume_target(step))
    return None


def choose_expression(text: str) -> str:
    lowered = text.lower()
    if any(word in lowered for word in ("sorry", "sad", "unfortunately")):
        return "sad"
    if any(word in lowered for word in ("great", "wonderful", "exciting", "love")):
        return "excited"
    if "?" in text:
        return "curious"
    return "happy"
