#pragma once

#include <M5Unified.h>

#include "face/Expression.h"

namespace firechan {

class EyeRenderer {
 public:
  void draw(M5Canvas& canvas, Expression expression, float gazeX, float gazeY,
            float blink, uint16_t background);

 private:
  void drawNormalEye(M5Canvas& canvas, int cx, int cy, int width, int height,
                     float gazeX, float gazeY, float blink, uint16_t background,
                     uint16_t eyeColor, bool pupil = true);
  void drawClosedEye(M5Canvas& canvas, int cx, int cy, bool happy, uint16_t color);
  void drawXEye(M5Canvas& canvas, int cx, int cy, uint16_t color);
  void thickLine(M5Canvas& canvas, int x0, int y0, int x1, int y1,
                 uint16_t color, int thickness = 4);
};

}  // namespace firechan

