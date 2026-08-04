from dataclasses import dataclass
from pathlib import Path
import os


@dataclass(frozen=True)
class Settings:
    token: str
    host: str
    port: int
    audio_dir: Path
    max_upload_bytes: int
    whisper_url: str
    ollama_url: str
    ollama_model: str
    piper_url: str
    personality: str

    @classmethod
    def from_environment(cls) -> "Settings":
        personality_path = Path(
            os.getenv(
                "EMBER_PERSONALITY_FILE",
                Path(__file__).resolve().parent.parent / "personality.txt",
            )
        )
        token = os.getenv("EMBER_TOKEN", "")
        if len(token) < 24:
            raise RuntimeError("EMBER_TOKEN must contain at least 24 characters")

        return cls(
            token=token,
            host=os.getenv("EMBER_HOST", "0.0.0.0"),
            port=int(os.getenv("EMBER_PORT", "8088")),
            audio_dir=Path(os.getenv("EMBER_AUDIO_DIR", "/var/lib/ember/audio")),
            max_upload_bytes=int(os.getenv("EMBER_MAX_UPLOAD_MB", "12")) * 1024 * 1024,
            whisper_url=os.getenv("WHISPER_URL", "http://127.0.0.1:8080").rstrip("/"),
            ollama_url=os.getenv("OLLAMA_URL", "http://127.0.0.1:11434").rstrip("/"),
            ollama_model=os.getenv("OLLAMA_MODEL", "llama3.2:3b"),
            piper_url=os.getenv("PIPER_URL", "http://127.0.0.1:5000").rstrip("/"),
            personality=personality_path.read_text(encoding="utf-8").strip(),
        )
