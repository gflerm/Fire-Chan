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
  // "time" and "status" are informational gateway commands.
  return AssistantAction::None;
}

}  // namespace

AssistantDirective AssistantDirectiveParser::parse(const char* expressionHint,
                                                   const char* actionHint) {
  AssistantDirective directive;
  directive.hasExpression = parseExpression(expressionHint, directive.expression);
  directive.action = parseAction(actionHint);
  return directive;
}

const char* AssistantDirectiveParser::actionName(AssistantAction action) {
  switch (action) {
    case AssistantAction::Sleep: return "sleep";
    case AssistantAction::Wake: return "wake";
    case AssistantAction::Mute: return "mute";
    case AssistantAction::Unmute: return "unmute";
    default: return "none";
  }
}

}  // namespace firechan
