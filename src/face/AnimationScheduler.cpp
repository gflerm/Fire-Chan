#include "face/AnimationScheduler.h"

#include <math.h>

namespace firechan {
namespace {
constexpr uint32_t kBlinkDurationMs = 180;
}

void AnimationScheduler::begin(uint32_t nowMs) {
  randomSeed(esp_random());
  scheduleBlink(nowMs);
  scheduleGaze(nowMs);
}

void AnimationScheduler::scheduleBlink(uint32_t nowMs) {
  nextBlinkMs_ = nowMs + random(2200, 6200);
}

void AnimationScheduler::scheduleGaze(uint32_t nowMs) {
  nextGazeMs_ = nowMs + random(1200, 3600);
  targetGazeX_ = random(-40, 41) / 100.0f;
  targetGazeY_ = random(-25, 26) / 100.0f;
}

AnimationFrame AnimationScheduler::update(uint32_t nowMs, Expression expression) {
  if (!blinking_ && static_cast<int32_t>(nowMs - nextBlinkMs_) >= 0) {
    blinking_ = true;
    blinkStartMs_ = nowMs;
  }

  float blink = 0.0f;
  if (blinking_) {
    const uint32_t elapsed = nowMs - blinkStartMs_;
    if (elapsed >= kBlinkDurationMs) {
      blinking_ = false;
      scheduleBlink(nowMs);
    } else {
      const float progress = elapsed / static_cast<float>(kBlinkDurationMs);
      blink = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
    }
  }

  if (static_cast<int32_t>(nowMs - nextGazeMs_) >= 0) scheduleGaze(nowMs);
  gazeX_ += (targetGazeX_ - gazeX_) * 0.035f;
  gazeY_ += (targetGazeY_ - gazeY_) * 0.035f;

  if (expression == Expression::Sleeping) blink = 1.0f;
  if (expression == Expression::Sleepy) blink = max(blink, 0.55f);

  AnimationFrame frame;
  frame.blink = constrain(blink, 0.0f, 1.0f);
  frame.idleGazeX = gazeX_;
  frame.idleGazeY = gazeY_;
  frame.mouthPhase = (sinf(nowMs * 0.018f) + 1.0f) * 0.5f;
  return frame;
}

}  // namespace firechan

