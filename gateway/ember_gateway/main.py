from pathlib import Path
from tempfile import NamedTemporaryFile
from uuid import uuid4
import time

from fastapi import Depends, FastAPI, File, Header, HTTPException, UploadFile
from fastapi.responses import FileResponse

from .commands import choose_expression, match_local_command
from .config import Settings
from .services import LocalVoiceServices
from . import __version__

settings = Settings.from_environment()
settings.audio_dir.mkdir(parents=True, exist_ok=True)
services = LocalVoiceServices(
    settings.whisper_url, settings.ollama_url, settings.ollama_model, settings.piper_url
)
app = FastAPI(title="Ember Local Voice Gateway", version=__version__)


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
    return {"ok": all(components.values()), "components": components}


@app.post("/v1/voice")
async def voice(file: UploadFile = File(...), _: None = Depends(authorize)) -> dict:
    if file.content_type not in ("audio/wav", "audio/x-wav", "application/octet-stream"):
        raise HTTPException(status_code=415, detail="A WAV recording is required")

    data = await file.read(settings.max_upload_bytes + 1)
    if len(data) > settings.max_upload_bytes:
        raise HTTPException(status_code=413, detail="Recording is too large")
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise HTTPException(status_code=400, detail="Recording is not a valid WAV file")

    temporary: Path | None = None
    try:
        with NamedTemporaryFile(prefix="ember-", suffix=".wav", delete=False) as handle:
            handle.write(data)
            temporary = Path(handle.name)
        transcript = await services.transcribe(temporary)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)

    if not transcript:
        raise HTTPException(status_code=422, detail="No speech was detected")

    command = match_local_command(transcript, settings.timezone)
    if command:
        reply, expression, action = command.reply, command.expression, command.action
    else:
        reply = await services.chat(transcript, settings.personality)
        expression, action = choose_expression(reply), None

    prune_audio()
    audio_id = uuid4().hex
    await services.synthesize(reply, settings.audio_dir / f"{audio_id}.wav")
    return {
        "transcript": transcript,
        "reply": reply,
        "expression": expression,
        "action": action,
        "audio_url": f"/v1/audio/{audio_id}.wav",
    }


@app.get("/v1/audio/{audio_id}.wav")
async def audio(audio_id: str, _: None = Depends(authorize)) -> FileResponse:
    if len(audio_id) != 32 or any(c not in "0123456789abcdef" for c in audio_id):
        raise HTTPException(status_code=404, detail="Audio not found")
    path = settings.audio_dir / f"{audio_id}.wav"
    if not path.is_file():
        raise HTTPException(status_code=404, detail="Audio not found")
    return FileResponse(path, media_type="audio/wav", filename="ember-response.wav")
