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
    timezone: str
    whisper_url: str
    llm_provider: str
    llm_fallback_to_ollama: bool
    ollama_url: str
    ollama_model: str
    gemini_api_key: str
    gemini_base_url: str
    gemini_model: str
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

        llm_provider = os.getenv("LLM_PROVIDER", "ollama").strip().lower()
        if llm_provider not in ("ollama", "gemini"):
            raise RuntimeError("LLM_PROVIDER must be 'ollama' or 'gemini'")
        gemini_api_key = os.getenv("GEMINI_API_KEY", "").strip()
        if llm_provider == "gemini" and not gemini_api_key:
            raise RuntimeError("GEMINI_API_KEY is required when LLM_PROVIDER=gemini")

        return cls(
            token=token,
            host=os.getenv("EMBER_HOST", "0.0.0.0"),
            port=int(os.getenv("EMBER_PORT", "8088")),
            audio_dir=Path(os.getenv("EMBER_AUDIO_DIR", "/var/lib/ember/audio")),
            max_upload_bytes=int(os.getenv("EMBER_MAX_UPLOAD_MB", "12")) * 1024 * 1024,
            timezone=os.getenv("EMBER_TIMEZONE", "Africa/Johannesburg"),
            whisper_url=os.getenv("WHISPER_URL", "http://127.0.0.1:8080").rstrip("/"),
            llm_provider=llm_provider,
            llm_fallback_to_ollama=os.getenv(
                "LLM_FALLBACK_TO_OLLAMA", "true"
            ).strip().lower() in ("1", "true", "yes", "on"),
            ollama_url=os.getenv("OLLAMA_URL", "http://127.0.0.1:11434").rstrip("/"),
            ollama_model=os.getenv("OLLAMA_MODEL", "llama3.2:3b"),
            gemini_api_key=gemini_api_key,
            gemini_base_url=os.getenv(
                "GEMINI_BASE_URL",
                "https://generativelanguage.googleapis.com/v1beta",
            ).rstrip("/"),
            gemini_model=os.getenv("GEMINI_MODEL", "gemini-3.5-flash-lite"),
            piper_url=os.getenv("PIPER_URL", "http://127.0.0.1:5000").rstrip("/"),
            personality=personality_path.read_text(encoding="utf-8").strip(),
        )
