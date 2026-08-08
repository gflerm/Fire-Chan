from dataclasses import dataclass
from datetime import datetime
import re
from zoneinfo import ZoneInfo

from .calc import parse_calculation


@dataclass(frozen=True)
class CommandResult:
    reply: str
    expression: str = "happy"
    action: str | None = None
    clear_session: bool = False
    repeat_last: bool = False
    query: str = ""
    seconds: float = 0.0


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
    if timer := _match_timer(normalized):
        return timer
    if search := _match_search(normalized):
        return search
    if weather := _match_weather(normalized):
        return weather
    if calc := _match_calc(text):
        return calc
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


# Reminder/timer deadline parsing: "5 minutes", "two hours", "30 seconds".
_NUMBER_WORDS = {
    "one": 1, "a": 1, "an": 1, "two": 2, "three": 3, "four": 4, "five": 5,
    "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10, "fifteen": 15,
    "twenty": 20, "thirty": 30, "forty": 40, "fortyfive": 45,
    "fifty": 50, "sixty": 60,
}


def _timer_seconds(text: str) -> float | None:
    """Extract a duration from \"in 5 minutes\" / \"for 30 seconds\"."""
    unit = 0.0
    match = re.search(r"\b(\d+)\s*(seconds?|minutes?|mins?|hours?|hrs?|hours?)\b", text)
    if match:
        value = int(match.group(1))
        unit_word = match.group(2)
    else:
        word = re.search(r"\b(one|a|an|two|three|four|five|six|seven|eight|nine|ten|"
                         r"fifteen|twenty|thirty|forty|fortyfive|fifty|sixty)\s*"
                         r"(seconds?|minutes?|mins?|hours?|hrs?)\b", text)
        if not word:
            return None
        value = _NUMBER_WORDS[word.group(1)]
        unit_word = word.group(2)
    if unit_word.startswith("second"):
        unit = 1.0
    elif unit_word.startswith("min"):
        unit = 60.0
    else:
        unit = 3600.0
    return value * unit


def _match_timer(text: str) -> CommandResult | None:
    if re.search(r"\b(timers?|remind|reminder|set (a )?timer|countdown)\b", text):
        # "set a timer for 5 minutes" / "remind me in 2 minutes to stretch"
        seconds = _timer_seconds(text)
        if seconds is not None:
            label = ""
            label_match = re.search(
                r"\bto (.+?)$|\b(?:remind me|set (?:a )?timer)\b.*\b(?:to )?(?:for )?(.+?)$",
                text,
            )
            if label_match:
                label = (label_match.group(1) or label_match.group(2) or "").strip()
                label = re.sub(r"\b(in|for) \d+( seconds?| minutes?| mins?| hours?)\b", "", label)
                label = re.sub(r"\b(set|a|the|me|for|in|to)\b", " ", label).strip()
            return CommandResult(
                f"Timer set for {int(seconds)} seconds.",
                "happy",
                "timer",
                seconds=seconds,
                query=label,
            )
        if re.search(r"\b(cancel|clear|remove|stop)\b", text):
            return CommandResult("I'll cancel your timers.", "neutral", "timer-cancel")
        if re.search(r"\b(list|show|what timers|active)\b", text):
            return CommandResult("I'll show your timers.", "happy", "timer-list")
        # A timer/reminder was asked for but no duration was understood.
        return CommandResult(
            "I can set a timer or reminder. How long should it be, in seconds or minutes?",
            "curious",
            "timer",
        )
    return None


def _match_search(text: str) -> CommandResult | None:
    """Explicit web-search intent: \"search for X\", \"look up X\"."""
    match = re.search(
        r"\b(search|look up|google|find out|check what|look into)\b(?: the web| online)?\s*"
        r"(?:for|about|on)?\s*(.+?)$",
        text,
    )
    if not match:
        return None
    query = match.group(2).strip()
    if not query or re.search(r"\b(help|options|it|them|that)\b", query) and len(query) < 6:
        return None
    return CommandResult(
        f"Let me look that up: {query}.",
        "curious",
        "search",
        query=query,
    )


def _match_weather(text: str) -> CommandResult | None:
    if re.search(
        r"\b(weather|forecast|temperature|rain(ing)?|snow(ing)?|sunny|cloudy|"
        r"hot|cold|what'?s it like outside)\b",
        text,
    ):
        place = ""
        place_match = re.search(r"\b(?:in|for|at)\s+([a-z][a-z' -]+?)\??\s*$", text)
        if place_match:
            place = re.sub(r"\s+", " ", place_match.group(1)).strip().strip("?") or ""
        return CommandResult("Let me check the weather.", "curious", "weather", query=place)
    return None


def _match_calc(text: str) -> CommandResult | None:
    """Deterministic arithmetic and unit-conversion questions."""
    answer = parse_calculation(text)
    if answer is None:
        return None
    return CommandResult(answer, "happy", "calc")


def choose_expression(text: str) -> str:
    lowered = text.lower()
    if any(word in lowered for word in ("sorry", "sad", "unfortunately")):
        return "sad"
    if any(word in lowered for word in ("great", "wonderful", "exciting", "love")):
        return "excited"
    if "?" in text:
        return "curious"
    return "happy"
