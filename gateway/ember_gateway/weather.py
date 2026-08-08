"""Keyless weather lookups via Open-Meteo.

Open-Meteo needs no API key. A fixed latitude/longitude (or a place name the
user configures) is used to fetch the current conditions, which Ember reads out
deterministically. Timezone-aware forecasts would come later; this slice reports
current temperature, condition code, wind, and apparent temperature.
"""

from __future__ import annotations

from dataclasses import dataclass

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


class WeatherClient:
    def __init__(
        self,
        latitude: float,
        longitude: float,
        place: str = "your area",
        base_url: str = "https://api.open-meteo.com/v1/forecast",
        timeout: float = 8.0,
        transport: httpx.AsyncBaseTransport | None = None,
    ):
        self.latitude = latitude
        self.longitude = longitude
        self.place = place
        self.base_url = base_url
        self.timeout = timeout
        self.transport = transport

    async def current(self) -> Weather:
        params = {
            "latitude": self.latitude,
            "longitude": self.longitude,
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
            place=self.place,
        )
