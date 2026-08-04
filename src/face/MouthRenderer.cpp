#include "face/MouthRenderer.h"

namespace firechan {
namespace {
constexpr uint16_t kMouth = TFT_WHITE;
constexpr uint16_t kMouthInside = 0x3000;
constexpr int kMouthY = 176;
}

void MouthRenderer::thickLine(M5Canvas& canvas, int x0, int y0, int x1, int y1,
                              uint16_t color, int thickness) {
  for (int offset = -thickness / 2; offset <= thickness / 2; ++offset) {
    canvas.drawLine(x0, y0 + offset, x1, y1 + offset, color);
  }
}

void MouthRenderer::smile(M5Canvas& canvas, bool large) {
  const int spread = large ? 45 : 34;
  const int drop = large ? 18 : 12;
  thickLine(canvas, 160 - spread, kMouthY - 4, 140, kMouthY + drop / 2, kMouth, 4);
  thickLine(canvas, 140, kMouthY + drop / 2, 160, kMouthY + drop, kMouth, 4);
  thickLine(canvas, 160, kMouthY + drop, 180, kMouthY + drop / 2, kMouth, 4);
  thickLine(canvas, 180, kMouthY + drop / 2, 160 + spread, kMouthY - 4, kMouth, 4);
}

void MouthRenderer::frown(M5Canvas& canvas) {
  thickLine(canvas, 122, kMouthY + 14, 145, kMouthY + 2, kMouth, 4);
  thickLine(canvas, 145, kMouthY + 2, 160, kMouthY - 2, kMouth, 4);
  thickLine(canvas, 160, kMouthY - 2, 175, kMouthY + 2, kMouth, 4);
  thickLine(canvas, 175, kMouthY + 2, 198, kMouthY + 14, kMouth, 4);
}

void MouthRenderer::draw(M5Canvas& canvas, Expression expression, float phase) {
  switch (expression) {
    case Expression::Happy:
      smile(canvas, false);
      break;
    case Expression::Excited:
      canvas.fillEllipse(160, kMouthY + 4, 45, 28, kMouthInside);
      canvas.drawEllipse(160, kMouthY + 4, 45, 28, kMouth);
      thickLine(canvas, 135, kMouthY - 2, 185, kMouthY - 2, kMouth, 3);
      break;
    case Expression::Sad:
      frown(canvas);
      break;
    case Expression::Surprised:
    case Expression::Alarmed:
      canvas.fillEllipse(160, kMouthY + 3, expression == Expression::Alarmed ? 25 : 18,
                         expression == Expression::Alarmed ? 30 : 23, kMouthInside);
      canvas.drawEllipse(160, kMouthY + 3, expression == Expression::Alarmed ? 25 : 18,
                         expression == Expression::Alarmed ? 30 : 23, kMouth);
      break;
    case Expression::Confused:
    case Expression::Thinking:
      thickLine(canvas, 122, kMouthY + 4, 143, kMouthY - 2, kMouth, 4);
      thickLine(canvas, 143, kMouthY - 2, 165, kMouthY + 5, kMouth, 4);
      thickLine(canvas, 165, kMouthY + 5, 198, kMouthY - 4, kMouth, 4);
      break;
    case Expression::Sleepy:
      canvas.drawEllipse(160, kMouthY + 5, 14, 10, kMouth);
      break;
    case Expression::Sleeping:
      thickLine(canvas, 143, kMouthY + 4, 177, kMouthY + 4, kMouth, 3);
      break;
    case Expression::Listening:
      canvas.drawEllipse(160, kMouthY + 2, 9, 7, kMouth);
      break;
    case Expression::Speaking: {
      const int height = 7 + static_cast<int>(phase * 22);
      canvas.fillEllipse(160, kMouthY + 3, 28, height, kMouthInside);
      canvas.drawEllipse(160, kMouthY + 3, 28, height, kMouth);
      break;
    }
    case Expression::Offline:
      thickLine(canvas, 126, kMouthY + 3, 194, kMouthY + 3, 0xBDF7, 4);
      break;
    case Expression::Error:
      thickLine(canvas, 120, kMouthY, 137, kMouthY - 8, kMouth, 4);
      thickLine(canvas, 137, kMouthY - 8, 154, kMouthY + 8, kMouth, 4);
      thickLine(canvas, 154, kMouthY + 8, 171, kMouthY - 8, kMouth, 4);
      thickLine(canvas, 171, kMouthY - 8, 200, kMouthY + 3, kMouth, 4);
      break;
    default:
      thickLine(canvas, 132, kMouthY + 3, 188, kMouthY + 3, kMouth, 4);
      break;
  }
}

}  // namespace firechan

