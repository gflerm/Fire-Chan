#pragma once

#include <Arduino.h>

#include "app/AppEvent.h"
#include "app/EventBus.h"
#include "face/Expression.h"

namespace firechan {

struct BehaviorAction {
  bool expressionChanged = false;
  Expression expression = Expression::Neutral;
  bool demoModeChanged = false;
  bool demoMode = true;
  bool toggleSound = false;
};

class BehaviorEngine {
 public:
  void begin(uint32_t nowMs);
  void tick(uint32_t nowMs, EventBus& events);
  BehaviorAction handle(const AppEvent& event);

  Expression expression() const { return activeExpression_; }
  bool demoMode() const { return demoMode_; }

 private:
  void setDemoMode(bool enabled, uint32_t nowMs, BehaviorAction& action);
  void setBaseExpression(Expression expression);
  void setTemporaryExpression(Expression expression, uint32_t nowMs,
                              uint32_t durationMs);
  Expression resolveExpression() const;
  void resolveAction(BehaviorAction& action);

  Expression baseExpression_ = Expression::Neutral;
  Expression temporaryExpression_ = Expression::Neutral;
  Expression activeExpression_ = Expression::Neutral;
  uint32_t temporaryUntilMs_ = 0;
  uint32_t nextDemoMs_ = 0;
  bool temporaryExpiryQueued_ = false;
  bool demoMode_ = true;
  bool faceDown_ = false;
  bool alarmActive_ = false;
  bool errorActive_ = false;
  bool listening_ = false;
  bool speaking_ = false;
  bool offline_ = false;
};

}  // namespace firechan

