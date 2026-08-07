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


def match_local_command(
    text: str,
    timezone: str = "Africa/Johannesburg",
    now: datetime | None = None,
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
    if re.search(r"\b(go to sleep|sleep now|good ?night)\b", normalized):
        return CommandResult("Good night. I'll be right here when you need me.", "sleepy", "sleep")
    if re.search(r"\b(wake up|good morning)\b", normalized):
        return CommandResult("I'm awake and ready.", "excited", "wake")
    if re.search(r"\b(mute|be quiet)\b", normalized):
        return CommandResult("Muted.", "neutral", "mute")
    if re.search(r"\b(unmute|you can speak)\b", normalized):
        return CommandResult("Voice is back on.", "happy", "unmute")
    if re.search(r"\b(status|how are you)\b", normalized):
        return CommandResult("I'm online and feeling bright.", "happy", "status")
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
