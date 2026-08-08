#pragma once

#include <Arduino.h>

#include "face/Expression.h"

namespace firechan {

enum class AssistantAction : uint8_t {
  None,
  Sleep,
  Wake,
  Mute,
  Unmute,
  Volume
};

struct AssistantDirective {
  bool hasExpression = false;
  Expression expression = Expression::Happy;
  AssistantAction action = AssistantAction::None;
  bool hasVolume = false;
  bool volumeAbsolute = false;
  uint8_t volumeTarget = 0;
  int8_t volumeDelta = 0;
};

class AssistantDirectiveParser {
 public:
  static AssistantDirective parse(const char* expressionHint,
                                  const char* actionHint);
  static const char* actionName(AssistantAction action);
};

}  // namespace firechan
