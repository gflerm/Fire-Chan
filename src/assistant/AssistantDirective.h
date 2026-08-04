#pragma once

#include <Arduino.h>

#include "face/Expression.h"

namespace firechan {

enum class AssistantAction : uint8_t {
  None,
  Sleep,
  Wake,
  Mute,
  Unmute
};

struct AssistantDirective {
  bool hasExpression = false;
  Expression expression = Expression::Happy;
  AssistantAction action = AssistantAction::None;
};

class AssistantDirectiveParser {
 public:
  static AssistantDirective parse(const char* expressionHint,
                                  const char* actionHint);
  static const char* actionName(AssistantAction action);
};

}  // namespace firechan
