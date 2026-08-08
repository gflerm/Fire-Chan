"""In-memory per-device timer and reminder store.

Timers live only on the Pi (never on the ESP32), matching how conversation memory
works. Each timer has an owner device, an optional label, and a due time. A
one-shot timer is removed when it becomes due; the gateway announces it on the
device's next voice turn (the Fire is push-to-talk, so the gateway cannot push an
alarm spontaneously without extra device polling).
"""

from __future__ import annotations

from dataclasses import dataclass
import threading
import time
import uuid


@dataclass
class Timer:
    id: str
    device_id: str
    label: str
    due_at: float  # time.monotonic() deadline
    created_at: float
    announced: bool = False


class TimerStore:
    def __init__(self, max_timers: int = 12):
        self.max_timers = max_timers
        self._timers: dict[str, Timer] = {}
        self._lock = threading.Lock()

    def add(self, device_id: str, label: str, seconds: float) -> Timer | None:
        now = time.monotonic()
        if seconds <= 0 or seconds > 24 * 3600:
            return None
        with self._lock:
            active = sum(1 for t in self._timers.values() if t.device_id == device_id)
            if active >= self.max_timers:
                return None
            timer = Timer(
                id=uuid.uuid4().hex[:8],
                device_id=device_id,
                label=label or "a timer",
                due_at=now + seconds,
                created_at=now,
            )
            self._timers[timer.id] = timer
            return timer

    def due(self, device_id: str, now: float | None = None) -> list[Timer]:
        """Return and remove timers that have elapsed for a device."""
        now = time.monotonic() if now is None else now
        done: list[Timer] = []
        with self._lock:
            for timer in list(self._timers.values()):
                if timer.device_id == device_id and now >= timer.due_at:
                    done.append(timer)
                    del self._timers[timer.id]
        return done

    def list(self, device_id: str) -> list[Timer]:
        now = time.monotonic()
        with self._lock:
            return [
                t for t in self._timers.values()
                if t.device_id == device_id and t.due_at > now
            ]

    def cancel(self, device_id: str, label: str | None = None) -> int:
        """Cancel timers for a device; optionally only matching a label."""
        target = label.lower() if label else None
        with self._lock:
            removed = 0
            for timer in list(self._timers.values()):
                if timer.device_id != device_id:
                    continue
                if target is not None and target not in timer.label.lower():
                    continue
                del self._timers[timer.id]
                removed += 1
            return removed
