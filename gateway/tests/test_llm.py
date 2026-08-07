import unittest

import httpx
import json

from ember_gateway.llm import (
    ConversationProvider,
    ConversationResult,
    FallbackProvider,
    GeminiProvider,
)


class FakeProvider(ConversationProvider):
    def __init__(self, name: str, reply: str = "", error: Exception | None = None):
        self.name = name
        self.reply = reply
        self.error = error
        self.last_messages: list[dict] | None = None

    async def chat(self, messages: list[dict]) -> ConversationResult:
        self.last_messages = messages
        if self.error:
            raise self.error
        return ConversationResult(self.reply, self.name)

    async def health(self) -> bool:
        return self.error is None


class GeminiProviderTests(unittest.IsolatedAsyncioTestCase):
    async def test_extracts_system_and_maps_assistant_role(self):
        def handler(request: httpx.Request) -> httpx.Response:
            self.assertEqual(request.headers["x-goog-api-key"], "test-key")
            self.assertNotIn("test-key", str(request.url))
            payload = json.loads(request.content)
            instruction = payload["systemInstruction"]["parts"][0]["text"]
            self.assertEqual(instruction, "You are Ember.")
            contents = payload["contents"]
            self.assertEqual(contents[0], {"role": "user", "parts": [{"text": "Hello"}]})
            self.assertEqual(
                contents[1], {"role": "model", "parts": [{"text": "Hi there"}]}
            )
            self.assertEqual(contents[2], {"role": "user", "parts": [{"text": "Again"}]})
            return httpx.Response(
                200,
                json={"candidates": [{"content": {"parts": [{"text": "Reply"}]}}]},
            )

        provider = GeminiProvider(
            "test-key",
            "gemini-test",
            "https://example.test/v1beta",
            httpx.MockTransport(handler),
        )
        messages = [
            {"role": "system", "content": "You are Ember."},
            {"role": "user", "content": "Hello"},
            {"role": "assistant", "content": "Hi there"},
            {"role": "user", "content": "Again"},
        ]
        reply = await provider.chat(messages)
        self.assertEqual(reply.text, "Reply")
        self.assertEqual(reply.provider, "gemini")

    async def test_falls_back_to_ollama_equivalent(self):
        provider = FallbackProvider(
            FakeProvider("gemini", error=httpx.ConnectError("offline")),
            FakeProvider("ollama", reply="Local reply"),
        )
        messages = [{"role": "system", "content": "Ember"}, {"role": "user", "content": "Hello"}]
        reply = await provider.chat(messages)
        self.assertEqual(reply.text, "Local reply")
        self.assertEqual(reply.provider, "ollama")

    async def test_ollama_passes_full_thread(self):
        provider = FakeProvider("ollama", reply="Thanks")
        messages = [
            {"role": "system", "content": "You are Ember."},
            {"role": "user", "content": "Hello"},
            {"role": "assistant", "content": "Hi"},
            {"role": "user", "content": "What about tomorrow?"},
        ]
        reply = await provider.chat(messages)
        self.assertEqual(provider.last_messages, messages)
        self.assertEqual(reply.provider, "ollama")


if __name__ == "__main__":
    unittest.main()