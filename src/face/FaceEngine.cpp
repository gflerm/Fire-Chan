#include "face/FaceEngine.h"

namespace firechan {
namespace {
constexpr uint32_t kFramePeriodMs = 33;
constexpr uint32_t kTransitionDurationMs = 280;

uint16_t blend565(uint16_t from, uint16_t to, float amount) {
  amount = constrain(amount, 0.0f, 1.0f);
  const int fr = (from >> 11) & 0x1F;
  const int fg = (from >> 5) & 0x3F;
  const int fb = from & 0x1F;
  const int tr = (to >> 11) & 0x1F;
  const int tg = (to >> 5) & 0x3F;
  const int tb = to & 0x1F;
  const int r = fr + static_cast<int>((tr - fr) * amount);
  const int g = fg + static_cast<int>((tg - fg) * amount);
  const int b = fb + static_cast<int>((tb - fb) * amount);
  return (r << 11) | (g << 5) | b;
}
}

FaceEngine::FaceEngine() : canvas_(&M5.Display) {}

bool FaceEngine::begin(uint8_t brightnessPercent) {
  M5.Display.setRotation(1);
  M5.Display.setBrightness(static_cast<uint8_t>(brightnessPercent * 255U / 100U));
  canvas_.setColorDepth(8);
  canvas_.setPsram(false);
  if (canvas_.createSprite(M5.Display.width(), M5.Display.height()) == nullptr) {
    Serial.println("[FACE] ERROR: internal sprite allocation failed");
    return false;
  }
  const uint32_t now = millis();
  scheduler_.begin(now);
  fpsWindowMs_ = now;
  Serial.printf("[FACE] canvas=%dx%d depth=8 internal_heap=%u\n", canvas_.width(),
                canvas_.height(), ESP.getFreeHeap());
  return true;
}

void FaceEngine::setExpression(Expression expression) {
  if (expression_ == expression) return;
  fromExpression_ = renderExpression_;
  expression_ = expression;
  transitionStartMs_ = millis();
  transitioning_ = true;
  Serial.printf("[FACE] expression=%s\n", expressionName(expression_));
}

void FaceEngine::setTilt(float x, float y) {
  tiltX_ += (constrain(x, -1.0f, 1.0f) - tiltX_) * 0.18f;
  tiltY_ += (constrain(y, -1.0f, 1.0f) - tiltY_) * 0.18f;
}

uint16_t FaceEngine::backgroundFor(Expression expression) const {
  switch (expression) {
    case Expression::Happy: return 0x0326;
    case Expression::Excited: return 0x04AA;
    case Expression::Sad: return 0x0015;
    case Expression::Surprised: return 0x4018;
    case Expression::Confused: return 0x4208;
    case Expression::Sleepy:
    case Expression::Sleeping: return 0x0842;
    case Expression::Listening: return 0x0250;
    case Expression::Thinking: return 0x280D;
    case Expression::Speaking: return 0x600C;
    case Expression::Alarmed: return 0xA800;
    case Expression::Offline: return 0x2945;
    case Expression::Error: return 0x7800;
    default: return 0x018C;
  }
}

void FaceEngine::drawDecorations(Expression expression, uint16_t background,
                                 uint32_t nowMs) {
  if (expression == Expression::Sleeping) {
    canvas_.setTextColor(TFT_CYAN, background);
    canvas_.setTextSize(2);
    canvas_.drawString("z", 255, 53);
    canvas_.drawString("Z", 275, 32);
  } else if (expression == Expression::Listening) {
    const int pulse = 4 + static_cast<int>((sinf(nowMs * 0.012f) + 1.0f) * 4);
    canvas_.drawCircle(292, 105, 10 + pulse, TFT_CYAN);
    canvas_.drawCircle(292, 105, 20 + pulse, TFT_CYAN);
  } else if (expression == Expression::Thinking) {
    canvas_.fillCircle(260, 172, 4, TFT_WHITE);
    canvas_.fillCircle(275, 158, 6, TFT_WHITE);
    canvas_.drawCircle(293, 139, 10, TFT_WHITE);
  } else if (expression == Expression::Excited) {
    canvas_.fillTriangle(25, 45, 34, 64, 14, 62, TFT_YELLOW);
    canvas_.fillTriangle(295, 45, 306, 62, 285, 64, TFT_YELLOW);
  }
}

void FaceEngine::reportFrameRate(uint32_t nowMs) {
  ++frameCount_;
  if (nowMs - fpsWindowMs_ >= 5000) {
    const float fps = frameCount_ * 1000.0f / (nowMs - fpsWindowMs_);
    Serial.printf("[FACE] fps=%.1f heap=%u expression=%s\n", fps, ESP.getFreeHeap(),
                  expressionName(expression_));
    frameCount_ = 0;
    fpsWindowMs_ = nowMs;
  }
}

void FaceEngine::update(uint32_t nowMs, bool demoMode) {
  if (nowMs - lastFrameMs_ < kFramePeriodMs) return;
  lastFrameMs_ = nowMs;

  float transitionProgress = 1.0f;
  float transitionBlink = 0.0f;
  if (transitioning_) {
    transitionProgress = (nowMs - transitionStartMs_) /
                         static_cast<float>(kTransitionDurationMs);
    if (transitionProgress >= 1.0f) {
      transitionProgress = 1.0f;
      transitioning_ = false;
      renderExpression_ = expression_;
    } else {
      if (transitionProgress >= 0.5f) renderExpression_ = expression_;
      transitionBlink = 1.0f - fabsf(transitionProgress * 2.0f - 1.0f);
    }
  }

  const AnimationFrame animation = scheduler_.update(nowMs, renderExpression_);
  float gazeX = constrain(tiltX_ + animation.idleGazeX, -1.0f, 1.0f);
  float gazeY = constrain(tiltY_ + animation.idleGazeY, -1.0f, 1.0f);
  const uint16_t background = transitioning_
                                  ? blend565(backgroundFor(fromExpression_),
                                             backgroundFor(expression_), transitionProgress)
                                  : backgroundFor(renderExpression_);
  const float blink = max(animation.blink, transitionBlink);

  canvas_.fillScreen(background);
  eyes_.draw(canvas_, renderExpression_, gazeX, gazeY, blink, background);
  mouth_.draw(canvas_, renderExpression_, animation.mouthPhase);
  drawDecorations(renderExpression_, background, nowMs);

  canvas_.setTextDatum(middle_center);
  canvas_.setTextSize(1);
  canvas_.setTextColor(0xC618, background);
  canvas_.drawString(expressionName(renderExpression_), 160, 221);
  canvas_.setTextDatum(top_left);
  canvas_.setTextColor(0x7BEF, background);
  canvas_.drawString(demoMode ? "AUTO A/B:step C:pause holdC:mute"
                              : "MANUAL A/B:step C:auto holdC:mute",
                     6, 228);
  canvas_.pushSprite(0, 0);
  reportFrameRate(nowMs);
}

}  // namespace firechan
