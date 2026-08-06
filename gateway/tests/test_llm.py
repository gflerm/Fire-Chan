import unittest

import httpx

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

    async def chat(self, transcript: str, personality: str) -> ConversationResult:
        if self.error:
            raise self.error
        return ConversationResult(self.reply, self.name)

    async def health(self) -> bool:
        return self.error is None


class GeminiProviderTests(unittest.IsolatedAsyncioTestCase):
    async def test_extracts_text_and_does_not_put_key_in_url(self):
        def handler(request: httpx.Request) -> httpx.Response:
            self.assertEqual(request.headers["x-goog-api-key"], "test-key")
            self.assertNotIn("test-key", str(request.url))
            payload = {
                "candidates": [
                    {"content": {"parts": [{"text": "Hello from Ember."}]}}
                ]
            }
            return httpx.Response(200, json=payload)

        provider = GeminiProvider(
            "test-key",
            "gemini-test",
            "https://example.test/v1beta",
            httpx.MockTransport(handler),
        )
        reply = await provider.chat("Hello", "You are Ember.")
        self.assertEqual(reply.text, "Hello from Ember.")
        self.assertEqual(reply.provider, "gemini")

    async def test_falls_back_to_ollama_equivalent(self):
        provider = FallbackProvider(
            FakeProvider("gemini", error=httpx.ConnectError("offline")),
            FakeProvider("ollama", reply="Local reply"),
        )
        reply = await provider.chat("Hello", "Ember")
        self.assertEqual(reply.text, "Local reply")
        self.assertEqual(reply.provider, "ollama")


if __name__ == "__main__":
    unittest.main()
