#include "face/Expression.h"

namespace firechan {

const char* expressionName(Expression expression) {
  static const char* names[] = {
      "Neutral",  "Happy",    "Excited",  "Sad",      "Surprised",
      "Confused", "Sleepy",   "Sleeping", "Listening", "Thinking",
      "Speaking", "Alarmed",  "Offline",  "Error"};
  const auto index = static_cast<uint8_t>(expression);
  return index < static_cast<uint8_t>(Expression::Count) ? names[index] : "Unknown";
}

Expression nextExpression(Expression expression) {
  const auto count = static_cast<uint8_t>(Expression::Count);
  return static_cast<Expression>((static_cast<uint8_t>(expression) + 1) % count);
}

Expression previousExpression(Expression expression) {
  const auto count = static_cast<uint8_t>(Expression::Count);
  return static_cast<Expression>((static_cast<uint8_t>(expression) + count - 1) % count);
}

}  // namespace firechan

