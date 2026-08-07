"""Per-device bounded conversation memory for the Ember gateway.

History is kept on the Pi, never sent from the ESP32. Each device is identified by
the anonymous ``X-Ember-Device`` header. A session holds only the most recent
turns and expires automatically after an idle period, which is how a "reset"
without an explicit command behaves. The user may also clear a session with the
"forget" command.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import threading
import time


@dataclass
class Session:
    turns: list[dict] = field(
        default_factory=list
    )  # each turn: {"user": str, "assistant": str}
    last_activity: float = field(default_factory=time.monotonic)


class SessionStore:
    def __init__(self, max_turns: int = 6, idle_seconds: float = 1800.0):
        self.max_turns = max_turns
        self.idle_seconds = idle_seconds
        self._sessions: dict[str, Session] = {}
        self._lock = threading.Lock()

    def _session(self, device_id: str, now: float) -> Session:
        session = self._sessions.get(device_id)
        if session is None:
            session = Session()
            self._sessions[device_id] = session
        elif now - session.last_activity > self.idle_seconds:
            # Start a fresh session after an idle period: no old history.
            session.turns = []
        session.last_activity = now
        return session

    def history(self, device_id: str, now: float | None = None) -> list[dict]:
        """Return stored turns as chat messages (user/assistant alternation)."""
        now = time.monotonic() if now is None else now
        messages: list[dict] = []
        with self._lock:
            for turn in self._session(device_id, now).turns:
                messages.append({"role": "user", "content": turn["user"]})
                messages.append({"role": "assistant", "content": turn["assistant"]})
        return messages

    def append(
        self,
        device_id: str,
        user: str,
        assistant: str,
        now: float | None = None,
    ) -> None:
        now = time.monotonic() if now is None else now
        with self._lock:
            session = self._session(device_id, now)
            session.turns.append({"user": user, "assistant": assistant})
            if len(session.turns) > self.max_turns:
                del session.turns[: len(session.turns) - self.max_turns]

    def clear(self, device_id: str) -> None:
        with self._lock:
            self._sessions.pop(device_id, None)