import unittest

from ember_gateway.commands import choose_expression, match_local_command


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

    def test_expression_selection(self):
        self.assertEqual(choose_expression("That's wonderful!"), "excited")
        self.assertEqual(choose_expression("Would you like to try?"), "curious")


if __name__ == "__main__":
    unittest.main()
