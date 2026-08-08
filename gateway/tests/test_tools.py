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


class _RoutingTransport(httpx.AsyncBaseTransport):
    """Returns a payload based on the request URL (geocode vs forecast)."""

    def __init__(self, geocode: dict | None, forecast: dict | None):
        self.geocode = geocode
        self.forecast = forecast

    async def handle_async_request(self, request):
        url = str(request.url)
        if "geocoding" in url and self.geocode is not None:
            return httpx.Response(200, json=self.geocode)
        return httpx.Response(200, json=self.forecast or {})


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

    def test_named_place_resolves_and_fetches(self):
        transport = _RoutingTransport(
            geocode={
                "results": [{"name": "Cape Town", "admin1": "Western Cape",
                             "country": "South Africa", "latitude": -33.9,
                             "longitude": 18.4}]
            },
            forecast={
                "current": {"temperature_2m": 18.2, "apparent_temperature": 16.0,
                            "weather_code": 61, "wind_speed_10m": 24.0}
            },
        )
        client = WeatherClient(location_file=None, transport=transport)
        result = asyncio.run(client.current(place="Cape Town"))
        self.assertEqual(result.temperature_c, 18.2)
        self.assertIn("Cape Town, Western Cape, South Africa", result.place)

    def test_unknown_place_raises(self):
        transport = _RoutingTransport(geocode={"results": []}, forecast={})
        client = WeatherClient(location_file=None, transport=transport)
        with self.assertRaises(Exception) as context:
            asyncio.run(client.current(place="Atlantis City"))
        self.assertIn("Atlantis", str(context.exception))

    def test_stored_location_used_before_ip(self):
        transport = _RoutingTransport(
            geocode=None,
            forecast={
                "current": {"temperature_2m": 9.0, "apparent_temperature": 7.0,
                            "weather_code": 0, "wind_speed_10m": 5.0}
            },
        )
        import json
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "loc.json"
            path.write_text(json.dumps({"latitude": -33.92, "longitude": 18.4,
                                        "place": "Cape Town"}))
            client = WeatherClient(location_file=str(path), transport=transport)
            result = asyncio.run(client.current())
        self.assertEqual(result.place, "Cape Town")
        self.assertEqual(result.temperature_c, 9.0)

    def test_ip_detection_parses_ip_api(self):
        from ember_gateway.weather import _parse_ip_payload
        location = _parse_ip_payload(
            "ip_api",
            {"status": "success", "lat": -33.92, "lon": 18.4,
             "city": "Cape Town", "regionName": "Western Cape", "country": "South Africa"},
        )
        self.assertIsNotNone(location)
        self.assertAlmostEqual(location.latitude, -33.92)
        self.assertIn("Cape Town", location.place)

    def test_ip_detection_falls_through_on_empty(self):
        from ember_gateway.weather import _parse_ip_payload
        self.assertIsNone(_parse_ip_payload("ip_api", {"status": "fail"}))


class VoiceServicesTests(unittest.TestCase):
    def _transcribe(self, payload: dict) -> str:
        from unittest import mock
        from pathlib import Path
        import tempfile
        import ember_gateway.services as services_module

        transport = _Transport(payload)
        services = services_module.LocalVoiceServices(
            "http://whisper.test", None, "http://piper.test"
        )

        class _Client(httpx.AsyncClient):
            def __init__(self, *args, **kwargs):
                kwargs["transport"] = transport
                super().__init__(*args, **kwargs)

        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as handle:
            handle.write(b"RIFF")
            path = Path(handle.name)

        async def _go():
            with mock.patch.object(services_module.httpx, "AsyncClient", _Client):
                return await services.transcribe(path)

        try:
            return asyncio.run(_go())
        finally:
            path.unlink(missing_ok=True)

    def test_blank_audio_marker_is_empty_transcript(self):
        self.assertEqual(self._transcribe({"text": "[BLANK_AUDIO]"}), "")

    def test_real_transcript_passes_through(self):
        self.assertEqual(
            self._transcribe({"text": "  what time is it  "}), "what time is it"
        )

    def test_empty_payload_is_empty_transcript(self):
        self.assertEqual(self._transcribe({"text": ""}), "")


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