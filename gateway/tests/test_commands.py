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
            device_status="fw=0.11.1;wifi=1;battery=80;sd_free_mb=2048",
        )
        self.assertEqual(result.action, "status")
        self.assertIn("connected to Wi-Fi", result.reply)
        self.assertIn("80% battery", result.reply)
        self.assertIn("firmware 0.11.1", result.reply)
        self.assertIn("2048", result.reply)

    def test_status_answers_battery_question(self):
        result = match_local_command(
            "What is your battery level?",
            device_status="fw=0.11.1;wifi=1;battery=80",
        )
        self.assertEqual(result.action, "status")
        self.assertIn("80% battery", result.reply)

    def test_status_answers_wifi_question(self):
        result = match_local_command(
            "Are you connected to Wi-Fi?",
            device_status="fw=0.11.1;wifi=1",
        )
        self.assertIn("connected to Wi-Fi", result.reply)

    def test_status_answers_storage_question(self):
        result = match_local_command(
            "How much storage do you have?",
            device_status="fw=0.11.1;sd_free_mb=1024;battery=na",
        )
        self.assertIn("1024", result.reply)
        self.assertNotIn("battery", result.reply)

    def test_status_empty_battery_omitted(self):
        result = match_local_command(
            "status", device_status="fw=0.11.1;battery=na;wifi=1"
        )
        self.assertIn("connected to Wi-Fi", result.reply)
        self.assertNotIn("battery", result.reply)

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

    def test_search_intent_extracts_query(self):
        result = match_local_command("Search the web for how to make sourdough")
        self.assertEqual(result.action, "search")
        self.assertIn("sourdough", result.query)

    def test_look_up_intent(self):
        result = match_local_command("look up the weather in London")
        self.assertEqual(result.action, "search")
        self.assertIn("london", result.query)

    def test_timer_intent_minutes(self):
        result = match_local_command("set a timer for 5 minutes")
        self.assertEqual(result.action, "timer")
        self.assertEqual(result.seconds, 300)

    def test_timer_intent_seconds_word(self):
        result = match_local_command("remind me in 30 seconds to stretch")
        self.assertEqual(result.action, "timer")
        self.assertEqual(result.seconds, 30)
        self.assertIn("stretch", result.query)

    def test_timer_list_intent(self):
        result = match_local_command("what timers are active")
        self.assertEqual(result.action, "timer-list")

    def test_timer_cancel_intent(self):
        result = match_local_command("cancel my timers")
        self.assertEqual(result.action, "timer-cancel")

    def test_weather_intent(self):
        result = match_local_command("what is the weather like?")
        self.assertEqual(result.action, "weather")

    def test_weather_intent_with_place(self):
        result = match_local_command("what is the weather like in Cape Town?")
        self.assertEqual(result.action, "weather")
        self.assertEqual(result.query, "cape town")

    def test_temperature_with_place(self):
        result = match_local_command("what is the temperature in Paris?")
        self.assertEqual(result.action, "weather")
        self.assertIn("paris", result.query)

    def test_calc_intent(self):
        result = match_local_command("what is 6 times 8?")
        self.assertEqual(result.action, "calc")
        self.assertEqual(result.reply, "That's 48.")

    def test_conversion_intent(self):
        result = match_local_command("convert 10 kilometers to miles")
        self.assertEqual(result.action, "calc")
        self.assertIn("6.2137", result.reply)

    def test_alarm_absolute_time(self):
        result = match_local_command("set an alarm for 5:30 AM")
        self.assertEqual(result.action, "alarm")
        self.assertGreater(result.seconds, 0)

    def test_alarm_relative(self):
        result = match_local_command("set an alarm in 10 minutes")
        self.assertEqual(result.action, "alarm")
        self.assertEqual(result.seconds, 600)

    def test_alarm_list(self):
        result = match_local_command("list my alarms")
        self.assertEqual(result.action, "alarm-list")

    def test_alarm_dismiss(self):
        result = match_local_command("dismiss my alarms")
        self.assertEqual(result.action, "alarm-dismiss")

    def test_search_shorts_are_not_intents(self):
        self.assertIsNone(match_local_command("search it"))
        self.assertIsNone(match_local_command("look that up"))


if __name__ == "__main__":
    unittest.main()