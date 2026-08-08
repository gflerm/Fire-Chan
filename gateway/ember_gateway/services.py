from pathlib import Path
import struct
import subprocess

import httpx

from .llm import ConversationProvider, ConversationResult

TARGET_AUDIO_RATE_HZ = 16_000


def canonical_pcm_wav(pcm: bytes, sample_rate: int, channels: int = 1) -> bytes:
    bits_per_sample = 16
    block_align = channels * bits_per_sample // 8
    byte_rate = sample_rate * block_align
    header = (
        b"RIFF"
        + struct.pack("<I", 36 + len(pcm))
        + b"WAVEfmt "
        + struct.pack(
            "<IHHIIHH",
            16,
            1,
            channels,
            sample_rate,
            byte_rate,
            block_align,
            bits_per_sample,
        )
        + b"data"
        + struct.pack("<I", len(pcm))
    )
    return header + pcm


def resample_to(pcm_wav: bytes, target_rate: int) -> bytes:
    if target_rate == 0:
        return pcm_wav
    try:
        result = subprocess.run(
            [
                "ffmpeg",
                "-y",
                "-loglevel",
                "error",
                "-i",
                "-",
                "-f",
                "s16le",
                "-ar",
                str(target_rate),
                "-ac",
                "1",
                "-",
            ],
            input=pcm_wav,
            capture_output=True,
            timeout=60,
        )
        if result.returncode == 0 and result.stdout:
            return canonical_pcm_wav(result.stdout, target_rate)
    except (FileNotFoundError, subprocess.SubprocessError, OSError):
        pass
    return pcm_wav


class LocalVoiceServices:
    def __init__(
        self,
        whisper_url: str,
        conversation: ConversationProvider,
        piper_url: str,
        audio_rate_hz: int = TARGET_AUDIO_RATE_HZ,
    ):
        self.whisper_url = whisper_url
        self.conversation = conversation
        self.piper_url = piper_url
        self.audio_rate_hz = audio_rate_hz

    async def transcribe(self, wav_path: Path) -> str:
        async with httpx.AsyncClient(timeout=120) as client:
            with wav_path.open("rb") as audio:
                response = await client.post(
                    f"{self.whisper_url}/inference",
                    files={"file": (wav_path.name, audio, "audio/wav")},
                    data={"response_format": "json", "temperature": "0.0"},
                )
        response.raise_for_status()
        payload = response.json()
        text = str(payload.get("text", "")).strip()
        # whisper.cpp returns bracketed markers for no-speech audio (e.g.
        # "[BLANK_AUDIO]"). Treat those as an empty transcript so the gateway
        # can respond gracefully instead of feeding them to the language model.
        if not text or (text.startswith("[") and text.endswith("]")):
            return ""
        return text

    async def chat(self, messages: list[dict]) -> ConversationResult:
        return await self.conversation.chat(messages)

    async def synthesize(self, text: str, output_path: Path) -> None:
        async with httpx.AsyncClient(timeout=120) as client:
            response = await client.post(f"{self.piper_url}/synthesize", json={"text": text})
        response.raise_for_status()
        output_path.write_bytes(resample_to(response.content, self.audio_rate_hz))

    async def health(self) -> dict[str, bool]:
        checks = {
            "whisper": f"{self.whisper_url}/",
            "piper": f"{self.piper_url}/info",
        }
        results: dict[str, bool] = {}
        async with httpx.AsyncClient(timeout=3) as client:
            for name, url in checks.items():
                try:
                    response = await client.get(url)
                    results[name] = response.status_code < 500
                except httpx.HTTPError:
                    results[name] = False
        results["conversation"] = await self.conversation.health()
        return results
