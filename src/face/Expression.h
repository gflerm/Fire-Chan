#pragma once

#include <Arduino.h>

namespace firechan {

enum class Expression : uint8_t {
  Neutral,
  Happy,
  Excited,
  Sad,
  Surprised,
  Confused,
  Sleepy,
  Sleeping,
  Listening,
  Thinking,
  Speaking,
  Alarmed,
  Offline,
  Error,
  Count
};

const char* expressionName(Expression expression);
Expression nextExpression(Expression expression);
Expression previousExpression(Expression expression);

}  // namespace firechan

