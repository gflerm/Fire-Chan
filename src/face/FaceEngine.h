#pragma once

#include <M5Unified.h>

#include "face/AnimationScheduler.h"
#include "face/EyeRenderer.h"
#include "face/Expression.h"
#include "face/MouthRenderer.h"

namespace firechan {

class FaceEngine {
 public:
  FaceEngine();
  bool begin();
  void setExpression(Expression expression);
  void setTilt(float x, float y);
  void update(uint32_t nowMs, bool demoMode);
  Expression expression() const { return expression_; }

 private:
  uint16_t backgroundFor(Expression expression) const;
  void drawDecorations(Expression expression, uint16_t background, uint32_t nowMs);
  void reportFrameRate(uint32_t nowMs);

  M5Canvas canvas_;
  EyeRenderer eyes_;
  MouthRenderer mouth_;
  AnimationScheduler scheduler_;
  Expression expression_ = Expression::Neutral;
  Expression renderExpression_ = Expression::Neutral;
  Expression fromExpression_ = Expression::Neutral;
  float tiltX_ = 0.0f;
  float tiltY_ = 0.0f;
  uint32_t transitionStartMs_ = 0;
  bool transitioning_ = false;
  uint32_t lastFrameMs_ = 0;
  uint32_t fpsWindowMs_ = 0;
  uint32_t frameCount_ = 0;
};

}  // namespace firechan
