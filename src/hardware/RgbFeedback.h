#pragma once

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#include "face/Expression.h"

namespace firechan {

class RgbFeedback {
 public:
  RgbFeedback();
  void begin();
  void setExpression(Expression expression);
  void update(uint32_t nowMs);

 private:
  uint32_t colorFor(Expression expression, uint8_t intensity) const;

  Adafruit_NeoPixel pixels_;
  Expression expression_ = Expression::Neutral;
  uint32_t lastUpdateMs_ = 0;
};

}  // namespace firechan

