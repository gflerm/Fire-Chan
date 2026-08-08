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
  Volume,
  SetAlarm
};

struct AssistantDirective {
  bool hasExpression = false;
  Expression expression = Expression::Happy;
  AssistantAction action = AssistantAction::None;
  bool hasVolume = false;
  bool volumeAbsolute = false;
  uint8_t volumeTarget = 0;
  int8_t volumeDelta = 0;
  // SetAlarm: the Unix timestamp (seconds) when the alarm should ring.
  uint32_t alarmTime = 0;
  char alarmLabel[32] = {};
};

class AssistantDirectiveParser {
 public:
  static AssistantDirective parse(const char* expressionHint,
                                  const char* actionHint);
  static const char* actionName(AssistantAction action);
};

}  // namespace firechan
