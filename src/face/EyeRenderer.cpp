#include "face/EyeRenderer.h"

namespace firechan {
namespace {
constexpr int kLeftEyeX = 91;
constexpr int kRightEyeX = 229;
constexpr int kEyeY = 100;
constexpr uint16_t kWhite = TFT_WHITE;
constexpr uint16_t kInk = 0x1082;
constexpr uint16_t kIris = 0x041F;
}

void EyeRenderer::thickLine(M5Canvas& canvas, int x0, int y0, int x1, int y1,
                            uint16_t color, int thickness) {
  for (int offset = -thickness / 2; offset <= thickness / 2; ++offset) {
    canvas.drawLine(x0, y0 + offset, x1, y1 + offset, color);
  }
}

void EyeRenderer::drawClosedEye(M5Canvas& canvas, int cx, int cy, bool happy,
                                uint16_t color) {
  if (happy) {
    thickLine(canvas, cx - 29, cy + 4, cx, cy - 8, color, 4);
    thickLine(canvas, cx, cy - 8, cx + 29, cy + 4, color, 4);
  } else {
    thickLine(canvas, cx - 30, cy, cx + 30, cy, color, 5);
  }
}

void EyeRenderer::drawXEye(M5Canvas& canvas, int cx, int cy, uint16_t color) {
  thickLine(canvas, cx - 22, cy - 20, cx + 22, cy + 20, color, 5);
  thickLine(canvas, cx - 22, cy + 20, cx + 22, cy - 20, color, 5);
}

void EyeRenderer::drawNormalEye(M5Canvas& canvas, int cx, int cy, int width,
                                int height, float gazeX, float gazeY, float blink,
                                uint16_t background, uint16_t eyeColor, bool pupil) {
  const int visibleHeight = max(3, static_cast<int>(height * (1.0f - blink)));
  if (visibleHeight <= 5) {
    drawClosedEye(canvas, cx, cy, false, eyeColor);
    return;
  }

  canvas.fillEllipse(cx, cy, width / 2, visibleHeight / 2, eyeColor);
  if (!pupil) return;
  const int maxX = max(1, width / 2 - 14);
  const int maxY = max(1, visibleHeight / 2 - 14);
  const int px = cx + static_cast<int>(constrain(gazeX, -1.0f, 1.0f) * maxX);
  const int py = cy + static_cast<int>(constrain(gazeY, -1.0f, 1.0f) * maxY);
  canvas.fillCircle(px, py, 13, kIris);
  canvas.fillCircle(px, py, 7, kInk);
  canvas.fillCircle(px - 3, py - 4, 3, kWhite);
}

void EyeRenderer::draw(M5Canvas& canvas, Expression expression, float gazeX,
                       float gazeY, float blink, uint16_t background) {
  int width = 70;
  int height = 74;
  int leftY = kEyeY;
  int rightY = kEyeY;

  if (expression == Expression::Happy) {
    drawClosedEye(canvas, kLeftEyeX, kEyeY, true, kWhite);
    drawClosedEye(canvas, kRightEyeX, kEyeY, true, kWhite);
    return;
  }
  if (expression == Expression::Sleeping) {
    drawClosedEye(canvas, kLeftEyeX, kEyeY, false, kWhite);
    drawClosedEye(canvas, kRightEyeX, kEyeY, false, kWhite);
    return;
  }
  if (expression == Expression::Error) {
    drawXEye(canvas, kLeftEyeX, kEyeY, kWhite);
    drawXEye(canvas, kRightEyeX, kEyeY, kWhite);
    return;
  }
  if (expression == Expression::Offline) {
    drawXEye(canvas, kLeftEyeX, kEyeY, 0xBDF7);
    drawXEye(canvas, kRightEyeX, kEyeY, 0xBDF7);
    return;
  }

  switch (expression) {
    case Expression::Excited:
    case Expression::Surprised:
    case Expression::Alarmed:
      width = 78;
      height = 92;
      break;
    case Expression::Confused:
      leftY -= 8;
      rightY += 7;
      break;
    case Expression::Listening:
      width = 76;
      gazeX = -0.65f;
      break;
    case Expression::Thinking:
      gazeX = 0.70f;
      gazeY = -0.65f;
      break;
    default:
      break;
  }

  drawNormalEye(canvas, kLeftEyeX, leftY, width, height, gazeX, gazeY, blink,
                background, kWhite);
  drawNormalEye(canvas, kRightEyeX, rightY, width, height, gazeX, gazeY, blink,
                background, kWhite);

  // Brows communicate emotion while keeping the eyes independently animated.
  switch (expression) {
    case Expression::Sad:
      thickLine(canvas, 58, 55, 119, 67, kWhite, 4);
      thickLine(canvas, 201, 67, 262, 55, kWhite, 4);
      break;
    case Expression::Confused:
      thickLine(canvas, 58, 63, 119, 54, kWhite, 4);
      thickLine(canvas, 201, 54, 262, 68, kWhite, 4);
      break;
    case Expression::Thinking:
      thickLine(canvas, 201, 59, 262, 49, kWhite, 4);
      break;
    case Expression::Alarmed:
      thickLine(canvas, 58, 65, 119, 49, kWhite, 4);
      thickLine(canvas, 201, 49, 262, 65, kWhite, 4);
      break;
    default:
      break;
  }
}

}  // namespace firechan

