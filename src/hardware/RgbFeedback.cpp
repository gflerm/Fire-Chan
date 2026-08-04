#include "hardware/RgbFeedback.h"

#include <math.h>

namespace firechan {
namespace {
constexpr uint8_t kRgbPin = 15;
constexpr uint8_t kRgbCount = 10;
}

RgbFeedback::RgbFeedback()
    : pixels_(kRgbCount, kRgbPin, NEO_GRB + NEO_KHZ800) {}

void RgbFeedback::begin(uint8_t brightnessPercent) {
  pixels_.begin();
  pixels_.setBrightness(static_cast<uint8_t>(brightnessPercent * 255U / 100U));
  pixels_.clear();
  pixels_.show();
  Serial.println("[RGB] expression lighting ready");
}

void RgbFeedback::setExpression(Expression expression) {
  expression_ = expression;
}

uint32_t RgbFeedback::colorFor(Expression expression, uint8_t intensity) const {
  auto scale = [intensity](uint8_t value) {
    return static_cast<uint8_t>((static_cast<uint16_t>(value) * intensity) / 255);
  };
  uint8_t r = 40, g = 130, b = 180;
  switch (expression) {
    case Expression::Happy: r = 255; g = 150; b = 20; break;
    case Expression::Excited: r = 255; g = 40; b = 180; break;
    case Expression::Sad: r = 20; g = 40; b = 210; break;
    case Expression::Surprised: r = 175; g = 40; b = 255; break;
    case Expression::Confused: r = 80; g = 210; b = 100; break;
    case Expression::Sleepy: r = 35; g = 25; b = 120; break;
    case Expression::Sleeping: r = 8; g = 5; b = 35; break;
    case Expression::Listening: r = 0; g = 190; b = 210; break;
    case Expression::Thinking: r = 100; g = 50; b = 200; break;
    case Expression::Speaking: r = 30; g = 220; b = 80; break;
    case Expression::Alarmed: r = 255; g = 35; b = 0; break;
    case Expression::Offline: r = 35; g = 35; b = 35; break;
    case Expression::Error: r = 255; g = 0; b = 0; break;
    default: break;
  }
  return pixels_.Color(scale(r), scale(g), scale(b));
}

void RgbFeedback::update(uint32_t nowMs) {
  if (nowMs - lastUpdateMs_ < 35) return;
  lastUpdateMs_ = nowMs;
  float wave = (sinf(nowMs * 0.0045f) + 1.0f) * 0.5f;
  uint8_t intensity = 90 + static_cast<uint8_t>(wave * 100);
  if (expression_ == Expression::Alarmed || expression_ == Expression::Error) {
    intensity = ((nowMs / 180) & 1) ? 230 : 25;
  }
  for (uint8_t i = 0; i < kRgbCount; ++i) {
    uint8_t pixelIntensity = intensity;
    if (expression_ == Expression::Excited) {
      pixelIntensity = 90 + ((i * 23 + nowMs / 8) % 150);
    }
    pixels_.setPixelColor(i, colorFor(expression_, pixelIntensity));
  }
  pixels_.show();
}

}  // namespace firechan
