import asyncio
import unittest

import httpx

from ember_gateway.search import SearchResult, WebSearchClient
from ember_gateway.timers import TimerStore
from ember_gateway.weather import WMO_CODES, Weather, WeatherClient


class _Transport(httpx.AsyncBaseTransport):
    def __init__(self, payload: dict):
        self.payload = payload

    async def handle_async_request(self, request):
        return httpx.Response(200, json=self.payload)


class WebSearchClientTests(unittest.TestCase):
    def test_search_parses_answer_and_topics(self):
        client = WebSearchClient(
            transport=_Transport(
                {
                    "Answer": "42 degrees",
                    "AbstractText": "Cape Town averages 20C in winter.",
                    "AbstractURL": "https://example.test/cape-town",
                    "RelatedTopics": [
                        {"Text": "Cape Town weather averaging 20C"},
                        {"Text": "Cape Town humidity around 70%"},
                        {"Topics": []},  # nested group skipped
                    ],
                }
            )
        )
        result = asyncio.run(client.search("cape town weather"))
        self.assertIsInstance(result, SearchResult)
        self.assertEqual(result.answer, "42 degrees")
        self.assertEqual(len(result.sources), 2)

    def test_grounding_text_empty_when_no_facts(self):
        result = SearchResult(query="nothing")
        self.assertEqual(WebSearchClient.grounding_text(result), "")

    def test_grounding_text_joins_real_sources(self):
        result = SearchResult(
            query="x",
            answer="An answer",
            abstract="A snippet.",
            sources=("one", "two"),
        )
        text = WebSearchClient.grounding_text(result)
        self.assertIn("An answer", text)
        self.assertIn("one", text)

    def test_search_raises_on_http_error(self):
        class _ErrorTransport(httpx.AsyncBaseTransport):
            async def handle_async_request(self, request):
                raise httpx.ConnectError("down")

        client = WebSearchClient(transport=_ErrorTransport())
        with self.assertRaises(httpx.HTTPError):
            asyncio.run(client.search("anything"))


class WeatherTests(unittest.TestCase):
    def test_describe_reports_condition_and_temperature(self):
        weather = Weather(
            temperature_c=18.2,
            apparent_c=16.0,
            wind_kmh=24.0,
            code=61,
            condition=WMO_CODES[61],
            place="Cape Town",
        )
        text = weather.describe()
        self.assertIn("rain", text)
        self.assertIn("Cape Town", text)
        self.assertIn("18", text)
        self.assertIn("kilometres", text)


class TimerStoreTests(unittest.TestCase):
    def test_add_and_due(self):
        store = TimerStore()
        timer = store.add("dev", "stretch", 0.1)
        self.assertIsNotNone(timer)
        due = store.due("dev", now=timer.created_at + 0.2)
        self.assertEqual(len(due), 1)
        self.assertEqual(due[0].label, "stretch")

    def test_not_due_yet(self):
        store = TimerStore()
        timer = store.add("dev", "stretch", 1000)
        self.assertEqual(store.due("dev", now=timer.created_at + 10), [])

    def test_list_only_active(self):
        store = TimerStore()
        store.add("dev", "stretch", 1000)
        timer2 = store.add("dev", "water", 1000)
        self.assertEqual(len(store.list("dev")), 2)
        self.assertIn("water", [t.label for t in store.list("dev")])

    def test_cancel_by_label(self):
        store = TimerStore()
        store.add("dev", "stretch", 1000)
        store.add("dev", "water the plants", 1000)
        removed = store.cancel("dev", "stretch")
        self.assertEqual(removed, 1)
        self.assertEqual(len(store.list("dev")), 1)

    def test_devices_isolated(self):
        store = TimerStore()
        store.add("one", "a", 1000)
        store.add("two", "b", 1000)
        self.assertEqual(len(store.list("one")), 1)
        self.assertEqual(store.list("one")[0].label, "a")

    def test_bounds(self):
        store = TimerStore(max_timers=1)
        store.add("dev", "first", 1000)
        over = store.add("dev", "second", 1000)
        self.assertIsNone(over)


if __name__ == "__main__":
    unittest.main()