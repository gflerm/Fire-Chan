from pathlib import Path
from tempfile import NamedTemporaryFile
from uuid import uuid4
import time

from fastapi import Depends, FastAPI, File, Header, HTTPException, UploadFile
from fastapi.responses import FileResponse

import httpx

from .commands import choose_expression, match_local_command
from .config import Settings
from .llm import FallbackProvider, GeminiProvider, OllamaProvider
from .memory import SessionStore
from .search import WebSearchClient
from .services import LocalVoiceServices
from .timers import TimerStore
from .weather import WeatherClient
from . import __version__

settings = Settings.from_environment()
settings.audio_dir.mkdir(parents=True, exist_ok=True)

sessions = SessionStore(
    max_turns=settings.session_max_turns,
    idle_seconds=settings.session_idle_seconds,
)

ollama = OllamaProvider(settings.ollama_url, settings.ollama_model)
if settings.llm_provider == "gemini":
    primary = GeminiProvider(
        settings.gemini_api_key,
        settings.gemini_model,
        settings.gemini_base_url,
    )
    conversation = FallbackProvider(
        primary,
        ollama if settings.llm_fallback_to_ollama else None,
    )
else:
    conversation = FallbackProvider(ollama)

services = LocalVoiceServices(
    settings.whisper_url,
    conversation,
    settings.piper_url,
    settings.audio_rate_hz,
)
search = WebSearchClient()
weather = WeatherClient(
    settings.weather_latitude,
    settings.weather_longitude,
    settings.weather_place,
    location_file=settings.weather_location_file,
)
timers = TimerStore()
app = FastAPI(title="Ember Local Voice Gateway", version=__version__)


async def handle_search(query: str) -> tuple[str, str, str]:
    """Run a web search and ground an answer in the results."""
    try:
        result = await search.search(query)
    except httpx.HTTPError:
        return "I could not reach the web to look that up.", "sad", None
    except (KeyError, ValueError):
        return "The search came back without anything useful.", "confused", None
    text = WebSearchClient.grounding_text(result)
    if not text:
        return "I could not find anything for that.", "sad", None
    messages = [
        {"role": "system", "content": settings.personality},
        {
            "role": "user",
            "content": f"{text}\n\nQuestion: {query}\n"
            "Answer briefly, in Ember's voice, from the search results only.",
        },
    ]
    reply = await services.chat(messages)
    if not reply.text.strip():
        return "I could not find anything for that.", "sad", None
    return reply.text, "curious", None


async def handle_weather(place: str | None = None) -> tuple[str, str, str]:
    """Fetch current conditions for a named place or the gateway location."""
    from .weather import LocationUnavailable

    try:
        current = await weather.current(place=place)
    except httpx.HTTPError:
        return "I could not reach the weather service right now.", "sad", None
    except LocationUnavailable as error:
        return str(error), "confused", None
    except (KeyError, ValueError):
        return "The weather service returned something I could not read.", "confused", None
    return current.describe(), "happy", None


def handle_timer_schedule(device_id: str, command: object) -> tuple[str, str]:
    if getattr(command, "seconds", 0) > 0:
        timer = timers.add(device_id, command.query, command.seconds)
        if timer is None:
            return "I could not set that timer; too many are already queued.", "sad"
        return f"I'll {timer.label or 'let you know'} in {command.seconds:g} seconds.", "happy"
    return "I couldn't understand how long the timer should be.", "confused"


def handle_timer_list(device_id: str) -> tuple[str, str]:
    pending = timers.list(device_id)
    if not pending:
        return "You have no active timers.", "neutral"
    remaining = [
        f"{int(t.due_at - time.monotonic()) // 60} minutes for {t.label}"
        for t in pending
    ]
    return "Active timers: " + "; ".join(remaining) + ".", "happy"


def handle_timer_cancel(device_id: str) -> tuple[str, str]:
    removed = timers.cancel(device_id)
    if removed == 0:
        return "There were no timers to cancel.", "neutral"
    return ("I cancelled your timer." if removed == 1
            else f"I cancelled {removed} timers.", "happy")


def authorize(x_ember_token: str = Header(default="")) -> None:
    if x_ember_token != settings.token:
        raise HTTPException(status_code=401, detail="Invalid Ember token")


def prune_audio(max_age_seconds: int = 3600) -> None:
    cutoff = time.time() - max_age_seconds
    for path in settings.audio_dir.glob("*.wav"):
        try:
            if path.stat().st_mtime < cutoff:
                path.unlink()
        except OSError:
            pass


@app.get("/health")
async def health(_: None = Depends(authorize)) -> dict:
    components = await services.health()
    return {
        "ok": all(components.values()),
        "components": components,
        "conversation_provider": conversation.name,
    }


@app.post("/v1/voice")
async def voice(
    file: UploadFile = File(...),
    _: None = Depends(authorize),
    x_ember_device: str = Header(default=""),
    x_ember_device_status: str = Header(default=""),
) -> dict:
    device_id = x_ember_device.strip() or "default"
    request_started = time.perf_counter()
    if file.content_type not in ("audio/wav", "audio/x-wav", "application/octet-stream"):
        raise HTTPException(status_code=415, detail="A WAV recording is required")

    data = await file.read(settings.max_upload_bytes + 1)
    if len(data) > settings.max_upload_bytes:
        raise HTTPException(status_code=413, detail="Recording is too large")
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise HTTPException(status_code=400, detail="Recording is not a valid WAV file")
    transcription_started = time.perf_counter()

    temporary: Path | None = None
    try:
        with NamedTemporaryFile(prefix="ember-", suffix=".wav", delete=False) as handle:
            handle.write(data)
            temporary = Path(handle.name)
        transcript = await services.transcribe(temporary)
        transcribed_at = time.perf_counter()
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)

    if not transcript:
        transcript = ""
        no_speech = True
    else:
        no_speech = False

    announcement: str | None = None
    due = timers.due(device_id)
    if due:
        labels = "; ".join(f"{t.label}" for t in due)
        announcement = (f"Your timer for {labels} is up. " if len(due) == 1
                        else f"Your timers are up: {labels}. ")

    command = match_local_command(
        transcript, settings.timezone, device_status=x_ember_device_status.strip()
    )
    if command:
        reply, expression, action = command.reply, command.expression, command.action
        reply_provider = "local-command"
        if command.repeat_last:
            previous = sessions.last_reply(device_id)
            if previous:
                reply, expression = previous, "neutral"
            else:
                reply = "There's nothing to repeat yet."
        elif command.action == "search" and command.query:
            reply, expression, action = await handle_search(command.query)
        elif command.action == "weather":
            reply, expression, action = await handle_weather(command.query or None)
        elif command.action == "timer":
            reply, expression = handle_timer_schedule(device_id, command)
        elif command.action == "timer-list":
            reply, expression = handle_timer_list(device_id)
        elif command.action == "timer-cancel":
            reply, expression = handle_timer_cancel(device_id)
        if command.clear_session:
            sessions.clear(device_id)
        else:
            sessions.append(device_id, transcript, reply)
    elif no_speech:
        reply = "Sorry, I didn't catch that. Could you say it again?"
        reply_provider = "local-command"
        expression, action = "confused", None
    else:
        messages = [{"role": "system", "content": settings.personality}]
        messages.extend(sessions.history(device_id))
        messages.append({"role": "user", "content": transcript})
        conversation_reply = await services.chat(messages)
        reply = conversation_reply.text
        reply_provider = conversation_reply.provider
        expression, action = choose_expression(reply), None
        sessions.append(device_id, transcript, reply)
    if announcement:
        reply = announcement + reply
    replied_at = time.perf_counter()

    prune_audio()
    audio_id = uuid4().hex
    await services.synthesize(reply, settings.audio_dir / f"{audio_id}.wav")
    synthesized_at = time.perf_counter()
    return {
        "transcript": transcript,
        "reply": reply,
        "expression": expression,
        "action": action,
        "audio_url": f"/v1/audio/{audio_id}.wav",
        "conversation_provider": reply_provider,
        "timings_ms": {
            "upload_validation": round((transcription_started - request_started) * 1000),
            "transcription": round((transcribed_at - transcription_started) * 1000),
            "conversation": round((replied_at - transcribed_at) * 1000),
            "synthesis": round((synthesized_at - replied_at) * 1000),
            "gateway_total": round((synthesized_at - request_started) * 1000),
        },
    }


@app.get("/v1/audio/{audio_id}.wav")
async def audio(audio_id: str, _: None = Depends(authorize)) -> FileResponse:
    if len(audio_id) != 32 or any(c not in "0123456789abcdef" for c in audio_id):
        raise HTTPException(status_code=404, detail="Audio not found")
    path = settings.audio_dir / f"{audio_id}.wav"
    if not path.is_file():
        raise HTTPException(status_code=404, detail="Audio not found")
    return FileResponse(path, media_type="audio/wav", filename="ember-response.wav")
