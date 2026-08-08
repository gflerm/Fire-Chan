#include "assistant/AssistantDirective.h"

#include <strings.h>

namespace firechan {
namespace {

bool equals(const char* value, const char* expected) {
  return value != nullptr && strcasecmp(value, expected) == 0;
}

bool parseExpression(const char* hint, Expression& expression) {
  if (equals(hint, "neutral")) expression = Expression::Neutral;
  else if (equals(hint, "happy")) expression = Expression::Happy;
  else if (equals(hint, "excited")) expression = Expression::Excited;
  else if (equals(hint, "sad")) expression = Expression::Sad;
  else if (equals(hint, "surprised")) expression = Expression::Surprised;
  else if (equals(hint, "curious") || equals(hint, "confused")) {
    // The face specification has Confused as its closest curious reaction.
    expression = Expression::Confused;
  } else if (equals(hint, "sleepy")) expression = Expression::Sleepy;
  else if (equals(hint, "sleeping")) expression = Expression::Sleeping;
  else return false;
  return true;
}

AssistantAction parseAction(const char* hint) {
  if (equals(hint, "sleep")) return AssistantAction::Sleep;
  if (equals(hint, "wake")) return AssistantAction::Wake;
  if (equals(hint, "mute")) return AssistantAction::Mute;
  if (equals(hint, "unmute")) return AssistantAction::Unmute;
  if (hint != nullptr && strncasecmp(hint, "volume=", 7) == 0) {
    return AssistantAction::Volume;
  }
  // "time", "status", and "help" are informational gateway commands.
  return AssistantAction::None;
}

bool parseVolume(const char* hint, AssistantDirective& directive) {
  // Accept "volume=40", "volume=40%", "volume=+10", and "volume=-10".
  if (hint == nullptr || strncasecmp(hint, "volume=", 7) != 0) return false;
  const char* value = hint + 7;
  if (*value == '\0') return false;
  bool negative = false;
  bool relative = false;
  if (*value == '+' || *value == '-') {
    relative = true;
    negative = *value == '-';
    ++value;
  }
  if (*value == '\0') return false;
  long parsed = 0;
  for (const char* cursor = value; *cursor != '\0' && *cursor != '%'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') return false;
    parsed = parsed * 10 + (*cursor - '0');
    if (parsed > 255) return false;
  }
  directive.hasVolume = true;
  directive.volumeAbsolute = !relative;
  if (relative) {
    directive.volumeDelta = static_cast<int8_t>(negative ? -parsed : parsed);
  } else {
    directive.volumeTarget = static_cast<uint8_t>(parsed);
  }
  return true;
}

}  // namespace

AssistantDirective AssistantDirectiveParser::parse(const char* expressionHint,
                                                   const char* actionHint) {
  AssistantDirective directive;
  directive.hasExpression = parseExpression(expressionHint, directive.expression);
  directive.action = parseAction(actionHint);
  if (directive.action == AssistantAction::Volume) {
    parseVolume(actionHint, directive);
  }
  return directive;
}

const char* AssistantDirectiveParser::actionName(AssistantAction action) {
  switch (action) {
    case AssistantAction::Sleep: return "sleep";
    case AssistantAction::Wake: return "wake";
    case AssistantAction::Mute: return "mute";
    case AssistantAction::Unmute: return "unmute";
    case AssistantAction::Volume: return "volume";
    default: return "none";
  }
}

}  // namespace firechan
