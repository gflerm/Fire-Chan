import unittest

from ember_gateway.memory import SessionStore


class SessionStoreTests(unittest.TestCase):
    def test_history_is_bounded_to_max_turns(self):
        store = SessionStore(max_turns=3, idle_seconds=3600)
        store.append("dev1", "one", "a")
        store.append("dev1", "two", "b")
        store.append("dev1", "three", "c")
        store.append("dev1", "four", "d")
        messages = store.history("dev1")
        self.assertEqual(
            messages,
            [
                {"role": "user", "content": "two"},
                {"role": "assistant", "content": "b"},
                {"role": "user", "content": "three"},
                {"role": "assistant", "content": "c"},
                {"role": "user", "content": "four"},
                {"role": "assistant", "content": "d"},
            ],
        )

    def test_clear_removes_all_history(self):
        store = SessionStore()
        store.append("dev", "one", "a")
        store.clear("dev")
        self.assertEqual(store.history("dev"), [])

    def test_idle_expiry_starts_fresh_session(self):
        store = SessionStore(max_turns=4, idle_seconds=60)
        store.append("dev", "user", "assistant", now=0.0)
        self.assertEqual(len(store.history("dev", now=10.0)), 2)
        self.assertEqual(store.history("dev", now=100.0), [])
        self.assertEqual(store.history("dev", now=110.0), [])

    def test_devices_are_isolated(self):
        store = SessionStore()
        store.append("one", "one-user", "one-assistant")
        store.append("two", "two-user", "two-assistant")
        history_one = store.history("one")
        self.assertIn({"role": "user", "content": "one-user"}, history_one)
        self.assertNotIn({"role": "user", "content": "two-user"}, history_one)

    def test_last_reply_returns_most_recent_assistant_turn(self):
        store = SessionStore()
        store.append("dev", "first user", "first assistant")
        store.append("dev", "second user", "second assistant")
        self.assertEqual(store.last_reply("dev"), "second assistant")

    def test_last_reply_none_for_fresh_session(self):
        store = SessionStore()
        self.assertIsNone(store.last_reply("dev"))
        self.assertIsNone(store.last_reply("missing"))

    def test_last_reply_none_after_clear(self):
        store = SessionStore()
        store.append("dev", "user", "assistant")
        store.clear("dev")
        self.assertIsNone(store.last_reply("dev"))


if __name__ == "__main__":
    unittest.main()