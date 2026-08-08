"""Keyless weather lookups via Open-Meteo, with place-name geocoding and
IP-based location auto-detection.

Lookup order when no place name is spoken:
1. The stored location on the gateway (persisted after a confirmed/auto detect).
2. IP-based geolocation, trying multiple free providers in order until one works.
3. The configured ``EMBER_WEATHER_LAT`` / ``EMBER_WEATHER_LON`` fallback.

A named place ("in Cape Town") always wins, is geocoded by Open-Meteo's keyless
search API, and is persisted so it becomes the default. All replies are
deterministic; an unresolved location raises an honest apology instead of
falling through to the model.
"""

from __future__ import annotations

from dataclasses import dataclass, asdict
from pathlib import Path
import json

import httpx

# WMO weather interpretation codes -> short English labels.
WMO_CODES: dict[int, str] = {
    0: "clear",
    1: "mainly clear",
    2: "partly cloudy",
    3: "overcast",
    45: "foggy",
    48: "icy fog",
    51: "light drizzle",
    53: "drizzle",
    55: "heavy drizzle",
    56: "freezing drizzle",
    57: "freezing drizzle",
    61: "light rain",
    63: "rain",
    65: "heavy rain",
    66: "freezing rain",
    67: "freezing rain",
    71: "light snow",
    73: "snow",
    75: "heavy snow",
    77: "snow grains",
    80: "light showers",
    81: "showers",
    82: "heavy showers",
    85: "snow showers",
    86: "snow showers",
    95: "thunderstorm",
    96: "thunderstorm with hail",
    99: "thunderstorm with hail",
}

# IP geolocation endpoints tried in order. Each returns lat/lon and a place name.
IP_ENDPOINTS = (
    # ipapi.co: {"latitude", "longitude", "city", "region", "country_name"}
    ("https://ipapi.co/json/", "ipapi"),
    # ipwho.is: {"latitude", "longitude", "city", "region", "country"}
    ("https://ipwho.is/", "ipwhois"),
    # ipinfo.io: {"loc": "lat,lon", "city", "region", "country"}
    ("https://ipinfo.io/json", "ipinfo"),
    # ip-api.com (HTTP only): {"status":"success","city","regionName","country"}
    ("http://ip-api.com/json/", "ip_api"),
)

GEOCODE_URL = "https://geocoding-api.open-meteo.com/v1/search"


class LocationUnavailable(Exception):
    """Raised when no usable location can be determined or geocoded."""


@dataclass(frozen=True)
class Location:
    latitude: float
    longitude: float
    place: str


@dataclass(frozen=True)
class Weather:
    temperature_c: float
    apparent_c: float
    wind_kmh: float
    code: int
    condition: str
    place: str

    def describe(self) -> str:
        text = f"It is {self.condition} in {self.place}, around {self.temperature_c:.0f} degrees"
        if abs(self.apparent_c - self.temperature_c) >= 2:
            text += f" (feels like {self.apparent_c:.0f})"
        if self.wind_kmh >= 10:
            text += f", with wind around {self.wind_kmh:.0f} kilometres per hour"
        return text + "."


def _parse_ip_payload(endpoint: str, data: dict) -> Location | None:
    """Extract a Location from a provider's JSON or return None."""
    if not isinstance(data, dict):
        return None
    if endpoint == "ipinfo":
        try:
            lat, lon = (float(part) for part in data["loc"].split(","))
        except (KeyError, ValueError, AttributeError):
            return None
        place = _smart_join(data.get("city"), data.get("region"), data.get("country"))
        if not place or abs(lat) > 90 or abs(lon) > 180:
            return None
        return Location(lat, lon, place)
    if endpoint == "ipapi":
        return _parse_ip_api(data)
    if endpoint == "ip_api":
        return _parse_ip_api(data)
    if endpoint == "ipwhois":
        lat = data.get("latitude")
        lon = data.get("longitude")
        if not isinstance(lat, (int, float)) or not isinstance(lon, (int, float)):
            return None
        place = _smart_join(data.get("city"), data.get("region"), data.get("country"))
        return Location(lat, lon, place or "your area")
    return None


def _parse_ip_api(data: dict) -> Location | None:
    if data.get("status") == "fail" or data.get("error"):
        return None
    lat = data.get("lat", data.get("latitude"))
    lon = data.get("lon", data.get("longitude"))
    if lat is None or lon is None:
        return None
    try:
        lat, lon = float(lat), float(lon)
    except (TypeError, ValueError):
        return None
    if abs(lat) > 90 or abs(lon) > 180:
        return None
    place = _smart_join(
        data.get("city"), data.get("regionName"), data.get("country")
    )
    return Location(lat, lon, place or "your area")


def _smart_join(*parts: object) -> str:
    seen: list[str] = []
    for part in parts:
        if not part:
            continue
        text = str(part).strip()
        if text and text not in seen and text.lower() != "unknown":
            seen.append(text)
    return ", ".join(seen)


class WeatherClient:
    def __init__(
        self,
        latitude: float = 0.0,
        longitude: float = 0.0,
        place: str = "your area",
        base_url: str = "https://api.open-meteo.com/v1/forecast",
        geocode_url: str = GEOCODE_URL,
        location_file: str | None = "/var/lib/weather_location.json",
        timeout: float = 8.0,
        transport: httpx.AsyncBaseTransport | None = None,
    ):
        self.latitude = latitude
        self.longitude = longitude
        self.place = place
        self.base_url = base_url
        self.geocode_url = geocode_url
        self.location_file = Path(location_file) if location_file else None
        self.timeout = timeout
        self.transport = transport

    # ---- location resolution -------------------------------------------------

    def _load_stored(self) -> Location | None:
        if self.location_file is None or not self.location_file.is_file():
            return None
        try:
            data = json.loads(self.location_file.read_text(encoding="utf-8"))
            return Location(
                latitude=float(data["latitude"]),
                longitude=float(data["longitude"]),
                place=str(data.get("place", "your area")),
            )
        except (OSError, ValueError, KeyError, TypeError):
            return None

    def _store(self, location: Location) -> None:
        if self.location_file is None:
            return
        try:
            self.location_file.parent.mkdir(parents=True, exist_ok=True)
            self.location_file.write_text(
                json.dumps(asdict(location)), encoding="utf-8"
            )
        except OSError:
            pass

    async def geocode(self, name: str) -> Location | None:
        """Resolve a spoken place name to coordinates via Open-Meteo."""
        params = {"name": name, "count": 1, "language": "en", "format": "json"}
        async with httpx.AsyncClient(
            timeout=self.timeout, transport=self.transport
        ) as client:
            response = await client.get(self.geocode_url, params=params)
        response.raise_for_status()
        results = response.json().get("results") or []
        if not results:
            return None
        result = results[0]
        name = result.get("name") or name
        admin1 = result.get("admin1")
        country = result.get("country")
        place = ", ".join(p for p in (name, admin1, country) if p)
        try:
            lat = float(result["latitude"])
            lon = float(result["longitude"])
        except (KeyError, ValueError, TypeError):
            return None
        return Location(latitude=lat, longitude=lon, place=place)

    async def detect_by_ip(self) -> Location | None:
        """Try each IP geolocation provider until one returns a location."""
        for url, flavor in IP_ENDPOINTS:
            try:
                async with httpx.AsyncClient(
                    timeout=self.timeout, transport=self.transport
                ) as client:
                    response = await client.get(url)
                if response.status_code >= 400:
                    continue
                location = _parse_ip_payload(flavor, response.json())
            except (httpx.HTTPError, ValueError, KeyError):
                location = None
            if location is not None:
                return location
        return None

    async def resolve_location(self, place: str | None = None) -> Location:
        """Pick a Location: named place, stored, IP-detected, or configured."""
        if place:
            named = await self.geocode(place)
            if named is None:
                raise LocationUnavailable(
                    f"I couldn't find {place}. Try a city name."
                )
            self._store(named)
            return named
        stored = self._load_stored()
        if stored is not None:
            return stored
        if self.latitude or self.longitude:
            return Location(self.latitude, self.longitude, self.place)
        detected = await self.detect_by_ip()
        if detected is None:
            raise LocationUnavailable(
                "I could not figure out where I am to check the weather."
            )
        self._store(detected)
        return detected

    # ---- current conditions --------------------------------------------------

    async def current(self, place: str | None = None) -> Weather:
        location = await self.resolve_location(place)
        params = {
            "latitude": location.latitude,
            "longitude": location.longitude,
            "current": "temperature_2m,apparent_temperature,weather_code,wind_speed_10m",
            "timezone": "auto",
        }
        async with httpx.AsyncClient(timeout=self.timeout, transport=self.transport) as client:
            response = await client.get(self.base_url, params=params)
        response.raise_for_status()
        current = response.json().get("current", {})
        code = int(current.get("weather_code", 0) or 0)
        return Weather(
            temperature_c=float(current.get("temperature_2m", 0.0) or 0.0),
            apparent_c=float(current.get("apparent_temperature", 0.0) or 0.0),
            wind_kmh=float(current.get("wind_speed_10m", 0.0) or 0.0),
            code=code,
            condition=WMO_CODES.get(code, "unknown conditions"),
            place=location.place,
        )