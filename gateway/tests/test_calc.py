import unittest

from ember_gateway.calc import (
    CalcError,
    convert,
    convert_temperature,
    evaluate_arithmetic,
    parse_calculation,
)


class ArithmeticTests(unittest.TestCase):
    def test_plain_addition(self):
        self.assertEqual(evaluate_arithmetic("2+2"), 4)

    def test_word_plus(self):
        self.assertEqual(parse_calculation("what is two plus two?"), "That's 4.")

    def test_times_words(self):
        self.assertEqual(parse_calculation("what is 6 times 8"), "That's 48.")

    def test_division_words(self):
        self.assertEqual(parse_calculation("what is 10 divided by 4"), "That's 2.5.")

    def test_symbol_power(self):
        self.assertEqual(evaluate_arithmetic("2**10"), 1024)

    def test_power_word(self):
        self.assertEqual(parse_calculation("what is 2 to the power of 10"), "That's 1024.")

    def test_precedence(self):
        self.assertEqual(evaluate_arithmetic("2+3*4"), 14.0)

    def test_parentheses(self):
        self.assertEqual(evaluate_arithmetic("(2+3)*4"), 20.0)

    def test_undefined_operator_rejected(self):
        with self.assertRaises(CalcError):
            evaluate_arithmetic("2**999999")  # absurdly large

    def test_non_numeric_rejected(self):
        with self.assertRaises(CalcError):
            evaluate_arithmetic("__import__('os')")

    def test_non_arithmetic_not_recognized(self):
        self.assertIsNone(parse_calculation("tell me about Saturn"))

    def test_division_by_zero_explained(self):
        result = parse_calculation("what is 5 divided by 0")
        self.assertIsNotNone(result)
        self.assertIn("cannot", result)


class ConversionTests(unittest.TestCase):
    def test_kilometers_to_miles(self):
        miles = convert(10, "kilometers", "miles")
        self.assertAlmostEqual(miles, 6.213711922, places=5)

    def test_inches_to_feet(self):
        self.assertAlmostEqual(convert(36, "inches", "feet"), 3.0, places=6)

    def test_pounds_to_kilograms(self):
        self.assertAlmostEqual(convert(10, "pounds", "kilograms"), 4.5359237, places=5)

    def test_whole_answer(self):
        self.assertEqual(
            parse_calculation("convert 2 miles to yards"),
            "2 miles is 3520 yards.",
        )

    def test_celsius_to_fahrenheit(self):
        self.assertEqual(parse_calculation("100 celsius in fahrenheit"), "100 celsius is 212 fahrenheit.")

    def test_mixed_units_rejected(self):
        with self.assertRaises(CalcError):
            convert(5, "kilograms", "meters")


if __name__ == "__main__":
    unittest.main()