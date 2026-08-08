"""Deterministic calculation and unit-conversion helper.

Parses spoken arithmetic and unit questions without relying on the language
model. Evaluation is restricted to literals and a fixed operator set so user
or transcript text can never execute arbitrary code. Failing to recognize an
input returns ``None`` so the normal model path can answer instead.
"""

from __future__ import annotations

import ast
import math
import operator
import re

# Whole-scale conversions share a linear value-to-base relation, so both
# directions use one factor. Temperature needs its own converters.
_UNITS: dict[str, dict[str, str]] = {
    "length": {
        "meter": "m", "meters": "m", "metre": "m", "metres": "m",
        "kilometer": "km", "kilometers": "km", "kilometre": "km", "kilometres": "km",
        "centimeter": "cm", "centimeters": "cm", "centimetre": "cm", "centimetres": "cm",
        "millimeter": "mm", "millimeters": "mm", "millimetre": "mm", "millimetres": "mm",
        "mile": "mile", "miles": "mile",
        "yard": "yard", "yards": "yard",
        "inch": "in", "inches": "in",
        "foot": "ft", "feet": "ft",
    },
    "mass": {
        "kilogram": "kg", "kilograms": "kg", "kilogramme": "kg", "kilogrammes": "kg",
        "gram": "g", "grams": "g", "gramme": "g", "grammes": "g",
        "milligram": "mg", "milligrams": "mg",
        "pound": "lb", "pounds": "lb",
        "ounce": "oz", "ounces": "oz",
        "tonne": "t", "tonnes": "t", "metric ton": "t", "metric tons": "t",
    },
    "volume": {
        "liter": "l", "liters": "l", "litre": "l", "litres": "l",
        "milliliter": "ml", "milliliters": "ml", "millilitre": "ml", "millilitres": "ml",
        "gallon": "gal", "gallons": "gal",
        "quart": "qt", "quarts": "qt",
        "pint": "pt", "pints": "pt",
        "cup": "cup", "cups": "cup",
    },
    "speed": {
        "meters per second": "mps", "metres per second": "mps",
        "meter per second": "mps", "metre per second": "mps",
        "kilometers per hour": "kph", "kilometres per hour": "kph",
        "kilometer per hour": "kph", "kilometre per hour": "kph",
        "miles per hour": "mph", "mile per hour": "mph",
    },
}

_TO_BASE: dict[str, dict[str, float]] = {
    "length": {
        "m": 1.0, "km": 1000.0, "cm": 0.01, "mm": 0.001,
        "mile": 1609.344, "yard": 0.9144, "in": 0.0254, "ft": 0.3048,
    },
    "mass": {
        "kg": 1000.0, "g": 1.0, "mg": 0.001, "lb": 453.59237,
        "oz": 28.349523125, "t": 1000000.0,
    },
    "volume": {
        "l": 1.0, "ml": 0.001, "gal": 3.785411784, "qt": 0.946352946,
        "pt": 0.473176473, "cup": 0.2365882365,
    },
    "speed": {
        "mps": 1.0, "kph": 0.2777777778, "mph": 0.44704,
    },
}

# Spoken operator words -> characters for the arithmetic parser.
_OPERATOR_WORDS = {
    "plus": "+", "add": "+", "and": "+",
    "minus": "-", "subtract": "-", "take away": "-",
    "times": "*", "multiplied by": "*", "multiply": "*", "by": "*",
    "divided by": "/", "over": "/",
    "to the power of": "**", "power": "**",
    "squared": "**2", "cubed": "**3",
}

_OPERATORS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.Div: operator.truediv,
    ast.Pow: operator.pow,
}


class CalcError(ValueError):
    """A recognized but unanswerable arithmetic question."""


def _evaluate_ast(node: ast.AST) -> float:
    if isinstance(node, ast.Expression):
        return _evaluate_ast(node.body)
    if isinstance(node, ast.Constant) and isinstance(node.value, (int, float)):
        return float(node.value)
    if isinstance(node, ast.BinOp) and type(node.op) in _OPERATORS:
        left = _evaluate_ast(node.left)
        right = _evaluate_ast(node.right)
        return _OPERATORS[type(node.op)](left, right)
    if isinstance(node, ast.UnaryOp) and isinstance(node.operand, ast.Constant):
        value = _evaluate_ast(node.operand)
        return value if isinstance(node.op, ast.UAdd) else -value
    raise CalcError("unsupported expression")


def evaluate_arithmetic(expression: str) -> float:
    """Safely evaluate a numeric-only expression string."""
    try:
        tree = ast.parse(expression, mode="eval")
    except (SyntaxError, ValueError) as error:
        raise CalcError("not arithmetic") from error
    try:
        value = _evaluate_ast(tree)
    except (CalcError, ZeroDivisionError, OverflowError) as error:
        raise CalcError("I cannot work that out") from error
    if not math.isfinite(value):
        raise CalcError("the result is too large for me")
    return value


def _normalize_unit(word: str) -> tuple[str, float] | None:
    """Return ``(base_unit, factor)`` for a spoken unit name."""
    normalized = word.strip().lower().replace("-", " ")
    for category, aliases in _UNITS.items():
        if normalized in aliases:
            base = aliases[normalized]
            return base, _TO_BASE[category][base]
    return None


def convert(value: float, source: str, target: str) -> float:
    """Convert ``value`` from ``source`` to ``target`` (same category)."""
    from_category = _find_category(source)
    to_category = _find_category(target)
    if from_category is None or from_category != to_category:
        raise CalcError(f"cannot convert {source} to {target}")
    from_factor = _normalize_unit(source)[1]
    to_factor = _normalize_unit(target)[1]
    in_base = value * from_factor
    return in_base / to_factor


def _find_category(word: str) -> str | None:
    normalized = word.strip().lower().replace("-", " ")
    for category, aliases in _UNITS.items():
        if any(alias == normalized for alias in aliases):
            return category
        if normalized in _TO_BASE[category]:
            return category
    return None


# Temperature relies on shifted scales, so it is separate from the linear table.
def convert_temperature(value: float, source: str, target: str) -> float:
    """Convert Celsius, Fahrenheit, or Kelvin temperatures."""
    source = source.lower().strip()
    target = target.lower().strip()
    known = {"celsius": "c", "centigrade": "c", "c": "c", "°c": "c",
             "fahrenheit": "f", "f": "f", "°f": "f",
             "kelvin": "k", "k": "k", "°k": "k"}
    src = known.get(source)
    tgt = known.get(target)
    if not src or not tgt:
        raise CalcError("unknown temperature unit")
    to_celsius = {
        "c": value,
        "f": (value - 32) * 5 / 9,
        "k": value - 273.15,
    }
    celsius = to_celsius[src]
    from_celsius = {
        "c": celsius,
        "f": celsius * 9 / 5 + 32,
        "k": celsius + 273.15,
    }
    return from_celsius[tgt]


_NUMBER_WORDS = {
    "zero": 0, "one": 1, "a": 1, "an": 1, "two": 2, "three": 3, "four": 4,
    "five": 5, "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10,
    "eleven": 11, "twelve": 12, "thirteen": 13, "fourteen": 14, "fifteen": 15,
    "sixteen": 16, "seventeen": 17, "eighteen": 18, "nineteen": 19,
    "twenty": 20, "thirty": 30, "forty": 40, "fifty": 50, "sixty": 60,
    "seventy": 70, "eighty": 80, "ninety": 90, "hundred": 100,
}

_ARITHMETIC_RE = re.compile(
    r"(?:what is|what's|compute|calculate|how much is|what does)\s*"
    r"(?P<expr>.+)$",
    re.IGNORECASE,
)


def _translate_expression(text: str) -> str | None:
    """Turn a spoken arithmetic phrase into a numeric expression string."""
    lowered = text.lower().strip().rstrip(".?")
    match = _ARITHMETIC_RE.search(lowered)
    if not match:
        return None
    phrase = match.group("expr")
    # Number words first, so "two plus two" -> "2 plus 2".
    for word, value in sorted(_NUMBER_WORDS.items(), key=lambda kv: len(kv[0]), reverse=True):
        phrase = re.sub(rf"\b{word}\b", str(value), phrase)
    for word, symbol in sorted(_OPERATOR_WORDS.items(), key=lambda kv: len(kv[0]), reverse=True):
        phrase = re.sub(rf"\b{word}\b", symbol, phrase)
    phrase = phrase.replace("^", "**").replace("x", "*").replace("×", "*")
    phrase = re.sub(r"[^\d+\-*/().\s]", "", phrase)
    expression = phrase.strip()
    if not re.search(r"\d", expression):
        return None
    return expression


def _format_number(value: float) -> str:
    if abs(value) >= 1e12 or (abs(value) < 1e-4 and value != 0):
        return f"{value:.3g}"
    if value == int(value):
        return str(int(value))
    return f"{value:.4f}".rstrip("0").rstrip(".")


def describe_arithmetic(text: str) -> str | None:
    """Return a spoken answer for an arithmetic question, or ``None``."""
    expression = _translate_expression(text)
    if expression is None:
        return None
    try:
        result = evaluate_arithmetic(expression)
    except CalcError as error:
        return f"That leaves {str(error)}."
    return f"That's {_format_number(result)}."


def describe_conversion(text: str) -> str | None:
    """Return a spoken answer for a conversion question, or ``None``."""
    lowered = text.lower().strip().rstrip("?!.")
    match = re.search(
        r"(?:convert\s+)?([\d.]*)\s*([a-z°]+)\s+(?:to|in|into)\s+([a-z°]+)",
        lowered,
    )
    if not match:
        return None
    value_text, source, target = match.groups()
    value: float
    if value_text:
        try:
            value = float(value_text)
        except ValueError:
            return None
    else:
        value = 1.0
    if _is_temperature(source) or _is_temperature(target):
        try:
            result = convert_temperature(value, source, target)
        except CalcError:
            return None
        return (
            f"{_format_number(value)} {source} is {_format_number(result)} {target}."
        )
    try:
        result = convert(value, source, target)
    except CalcError:
        return None
    if result is None:
        return None
    return (
        f"{_format_number(value)} {source} is {_format_number(result)} {target}."
    )


def _is_temperature(unit: str) -> bool:
    normalized = unit.lower().strip()
    return normalized in {
        "c", "f", "k", "celsius", "centigrade", "fahrenheit", "kelvin", "°c", "°f",
    }


def parse_calculation(text: str) -> str | None:
    """Dispatch an arithmetic or conversion question to a deterministic answer."""
    answer = describe_arithmetic(text)
    if answer is not None:
        return answer
    answer = describe_conversion(text)
    if answer is not None:
        return answer
    return None