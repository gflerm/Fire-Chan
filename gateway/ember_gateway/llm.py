from __future__ import annotations

from abc import ABC, abstractmethod
from dataclasses import dataclass
import logging

import httpx


LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class ConversationResult:
    text: str
    provider: str


class ConversationProvider(ABC):
    name: str

    @abstractmethod
    async def chat(self, transcript: str, personality: str) -> ConversationResult:
        raise NotImplementedError

    @abstractmethod
    async def health(self) -> bool:
        raise NotImplementedError


class OllamaProvider(ConversationProvider):
    name = "ollama"

    def __init__(self, base_url: str, model: str):
        self.base_url = base_url
        self.model = model

    async def chat(self, transcript: str, personality: str) -> ConversationResult:
        async with httpx.AsyncClient(timeout=180) as client:
            response = await client.post(
                f"{self.base_url}/api/chat",
                json={
                    "model": self.model,
                    "stream": False,
                    "messages": [
                        {"role": "system", "content": personality},
                        {"role": "user", "content": transcript},
                    ],
                    "options": {"temperature": 0.7, "num_predict": 100},
                },
            )
        response.raise_for_status()
        reply = str(response.json()["message"]["content"]).strip()
        if not reply:
            raise RuntimeError("Ollama returned an empty reply")
        return ConversationResult(reply, self.name)

    async def health(self) -> bool:
        try:
            async with httpx.AsyncClient(timeout=3) as client:
                response = await client.get(f"{self.base_url}/api/tags")
            return response.is_success
        except httpx.HTTPError:
            return False


class GeminiProvider(ConversationProvider):
    name = "gemini"

    def __init__(
        self,
        api_key: str,
        model: str,
        base_url: str = "https://generativelanguage.googleapis.com/v1beta",
        transport: httpx.AsyncBaseTransport | None = None,
    ):
        self.api_key = api_key
        self.model = model
        self.base_url = base_url.rstrip("/")
        self.transport = transport

    async def chat(self, transcript: str, personality: str) -> ConversationResult:
        async with httpx.AsyncClient(timeout=30, transport=self.transport) as client:
            response = await client.post(
                f"{self.base_url}/models/{self.model}:generateContent",
                headers={"x-goog-api-key": self.api_key},
                json={
                    "systemInstruction": {"parts": [{"text": personality}]},
                    "contents": [
                        {"role": "user", "parts": [{"text": transcript}]}
                    ],
                    "generationConfig": {
                        "maxOutputTokens": 120,
                        "thinkingConfig": {"thinkingLevel": "minimal"},
                    },
                },
            )
        response.raise_for_status()
        payload = response.json()
        candidates = payload.get("candidates", [])
        if not candidates:
            raise RuntimeError("Gemini returned no response candidate")
        parts = candidates[0].get("content", {}).get("parts", [])
        reply = "".join(
            str(part.get("text", ""))
            for part in parts
            if not part.get("thought", False)
        ).strip()
        if not reply:
            raise RuntimeError("Gemini returned an empty reply")
        return ConversationResult(reply, self.name)

    async def health(self) -> bool:
        try:
            async with httpx.AsyncClient(timeout=3, transport=self.transport) as client:
                response = await client.get(
                    f"{self.base_url}/models/{self.model}",
                    headers={"x-goog-api-key": self.api_key},
                )
            return response.is_success
        except httpx.HTTPError:
            return False


class FallbackProvider(ConversationProvider):
    def __init__(
        self,
        primary: ConversationProvider,
        fallback: ConversationProvider | None = None,
    ):
        self.primary = primary
        self.fallback = fallback
        self.name = primary.name

    async def chat(self, transcript: str, personality: str) -> ConversationResult:
        try:
            return await self.primary.chat(transcript, personality)
        except (httpx.HTTPError, RuntimeError, KeyError, ValueError) as error:
            if self.fallback is None:
                raise
            LOGGER.warning(
                "Conversation provider %s failed (%s); using %s",
                self.primary.name,
                type(error).__name__,
                self.fallback.name,
            )
            return await self.fallback.chat(transcript, personality)

    async def health(self) -> bool:
        if await self.primary.health():
            return True
        return self.fallback is not None and await self.fallback.health()
