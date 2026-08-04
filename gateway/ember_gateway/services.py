from pathlib import Path
import httpx


class LocalVoiceServices:
    def __init__(self, whisper_url: str, ollama_url: str, ollama_model: str, piper_url: str):
        self.whisper_url = whisper_url
        self.ollama_url = ollama_url
        self.ollama_model = ollama_model
        self.piper_url = piper_url

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
        return str(payload.get("text", "")).strip()

    async def chat(self, transcript: str, personality: str) -> str:
        async with httpx.AsyncClient(timeout=180) as client:
            response = await client.post(
                f"{self.ollama_url}/api/chat",
                json={
                    "model": self.ollama_model,
                    "stream": False,
                    "messages": [
                        {"role": "system", "content": personality},
                        {"role": "user", "content": transcript},
                    ],
                    "options": {"temperature": 0.7, "num_predict": 100},
                },
            )
        response.raise_for_status()
        return str(response.json()["message"]["content"]).strip()

    async def synthesize(self, text: str, output_path: Path) -> None:
        async with httpx.AsyncClient(timeout=120) as client:
            response = await client.post(f"{self.piper_url}/synthesize", json={"text": text})
        response.raise_for_status()
        output_path.write_bytes(response.content)

    async def health(self) -> dict[str, bool]:
        checks = {
            "whisper": f"{self.whisper_url}/",
            "ollama": f"{self.ollama_url}/api/tags",
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
        return results
