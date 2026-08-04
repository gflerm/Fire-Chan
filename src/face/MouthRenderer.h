#pragma once

#include <M5Unified.h>

#include "face/Expression.h"

namespace firechan {

class MouthRenderer {
 public:
  void draw(M5Canvas& canvas, Expression expression, float phase);

 private:
  void thickLine(M5Canvas& canvas, int x0, int y0, int x1, int y1,
                 uint16_t color, int thickness = 4);
  void smile(M5Canvas& canvas, bool large);
  void frown(M5Canvas& canvas);
};

}  // namespace firechan

