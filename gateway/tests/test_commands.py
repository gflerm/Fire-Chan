import unittest
from datetime import datetime, timedelta, timezone

from ember_gateway.commands import choose_expression, match_local_command, parse_device_status


class CommandTests(unittest.TestCase):
    def test_name(self):
        result = match_local_command("What is your name?")
        self.assertIsNotNone(result)
        self.assertEqual(result.expression, "happy")
        self.assertIn("Ember", result.reply)

    def test_sleep_action(self):
        result = match_local_command("Please go to sleep")
        self.assertEqual(result.action, "sleep")
        self.assertEqual(result.expression, "sleepy")

    def test_unknown_phrase_uses_model(self):
        self.assertIsNone(match_local_command("Tell me about Saturn"))

    def test_time_uses_configured_clock(self):
        now = datetime(2026, 8, 4, 18, 47, tzinfo=timezone(timedelta(hours=2)))
        result = match_local_command("What time is it?", now=now)
        self.assertEqual(result.reply, "It's 6:47 PM.")
        self.assertEqual(result.action, "time")

    def test_date_uses_configured_clock(self):
        now = datetime(2026, 8, 4, 18, 47, tzinfo=timezone(timedelta(hours=2)))
        result = match_local_command("What is the date?", now=now)
        self.assertEqual(result.reply, "Today is Tuesday, 04 August 2026.")
        self.assertEqual(result.action, "time")

    def test_forget_marks_clear_session(self):
        result = match_local_command("Forget our conversation")
        self.assertIsNotNone(result)
        self.assertTrue(result.clear_session)
        self.assertEqual(result.action, None)

    def test_start_over_marks_clear_session(self):
        result = match_local_command("Let's start over")
        self.assertIsNotNone(result)
        self.assertTrue(result.clear_session)

    def test_named_day_uses_clock(self):
        now = datetime(2026, 8, 4, 9, 0, tzinfo=timezone(timedelta(hours=2)))
        result = match_local_command("What day is it?", now=now)
        self.assertEqual(result.reply, "Today is Tuesday, 04 August 2026.")

    def test_expression_selection(self):
        self.assertEqual(choose_expression("That's wonderful!"), "excited")
        self.assertEqual(choose_expression("Would you like to try?"), "curious")

    def test_help_command(self):
        result = match_local_command("What can you do?")
        self.assertIsNotNone(result)
        self.assertEqual(result.action, "help")
        self.assertIn("volume", result.reply)
        self.assertIn("repeat", result.reply)

    def test_repeat_marks_repeat_last(self):
        result = match_local_command("Can you say that again?")
        self.assertIsNotNone(result)
        self.assertTrue(result.repeat_last)

    def test_volume_absolute_target(self):
        result = match_local_command("Set the volume to 40 percent")
        self.assertEqual(result.action, "volume=40")
        self.assertIn("40", result.reply)

    def test_volume_absolute_clamped(self):
        result = match_local_command("Set the volume to 150")
        self.assertEqual(result.action, "volume=100")

    def test_volume_up_step(self):
        result = match_local_command("Turn it up")
        self.assertEqual(result.action, "volume=+10")

    def test_volume_down_step(self):
        result = match_local_command("Make it quieter")
        self.assertEqual(result.action, "volume=-10")

    def test_status_with_device_facts(self):
        result = match_local_command(
            "How are you?",
            device_status="fw=0.11.1;wifi=1;battery=80",
        )
        self.assertEqual(result.action, "status")
        self.assertIn("connected to Wi-Fi", result.reply)
        self.assertIn("80% battery", result.reply)
        self.assertIn("firmware 0.11.1", result.reply)

    def test_status_without_facts(self):
        result = match_local_command("How are you?")
        self.assertEqual(result.reply, "I'm online and feeling bright.")

    def test_status_ignores_unknown_facts(self):
        result = match_local_command("status", device_status="foo=bar;w=2")
        self.assertEqual(result.reply, "I'm online and feeling bright.")

    def test_parse_device_status(self):
        facts = parse_device_status("fw=0.11.0;wifi=1;sd_free_mb=2048;battery=75")
        self.assertEqual(facts["fw"], "0.11.0")
        self.assertEqual(facts["wifi"], "1")
        self.assertEqual(facts["sd_free_mb"], "2048")
        self.assertEqual(facts["battery"], "75")

    def test_parse_device_status_ignores_malformed(self):
        self.assertEqual(parse_device_status("nor valid"), {})


if __name__ == "__main__":
    unittest.main()