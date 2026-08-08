"""Keyless web search for grounding Ember's answers.

The gateway does not have search keys. DuckDuckGo's Instant Answer API needs no
account, so it is used to fetch a short answer, a snippet, and up to a few
related topics for a query. Results are passed into the conversation so the
model can phrase a natural reply from real sources instead of guessing.
"""

from __future__ import annotations

from dataclasses import dataclass

import httpx


@dataclass(frozen=True)
class SearchResult:
    query: str
    answer: str = ""
    abstract: str = ""
    url: str = ""
    sources: tuple[str, ...] = ()


class WebSearchClient:
    def __init__(
        self,
        base_url: str = "https://api.duckduckgo.com/",
        timeout: float = 8.0,
        transport: httpx.AsyncBaseTransport | None = None,
    ):
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout
        self.transport = transport

    async def search(self, query: str) -> SearchResult:
        params = {
            "q": query,
            "format": "json",
            "no_html": 1,
            "skip_disambig": 1,
            "no_redirect": 1,
        }
        async with httpx.AsyncClient(timeout=self.timeout, transport=self.transport) as client:
            response = await client.get(self.base_url, params=params)
        response.raise_for_status()
        payload = response.json()
        answer = str(payload.get("Answer") or "").strip()
        abstract = str(payload.get("AbstractText") or "").strip()
        url = str(payload.get("AbstractURL") or "").strip()

        sources: list[str] = []
        for topic in payload.get("RelatedTopics") or []:
            if not isinstance(topic, dict) or not topic.get("Text"):
                continue
            if topic.get("Topics"):  # nested category group; skip
                continue
            title = str(topic.get("Text", "")).strip()
            if title:
                sources.append(title)
            if len(sources) >= 3:
                break

        return SearchResult(
            query=query,
            answer=answer,
            abstract=abstract,
            url=url,
            sources=tuple(sources),
        )

    @staticmethod
    def grounding_text(result: SearchResult) -> str:
        """Compact context block appended to the system prompt."""
        if not result.answer and not result.abstract and not result.sources:
            return ""
        lines = ["Web search results for the question:"]
        if result.answer:
            lines.append(f"- Answer: {result.answer}")
        if result.abstract:
            lines.append(f"- {result.abstract}")
        for source in result.sources:
            lines.append(f"- {source}")
        return "\n".join(lines)
