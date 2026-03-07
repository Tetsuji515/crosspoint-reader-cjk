#include "ExternalGlyphLayout.h"

#include <algorithm>

ExternalGlyphLayout computeExternalGlyphLayout(int cursorX, int lineTopY, const ExternalFont& font,
                                               const ExternalGlyphMetrics& metrics, int advanceOverride) {
  ExternalGlyphLayout layout{};
  layout.baselineY = lineTopY + font.getAscender();
  layout.drawX = cursorX + metrics.left;
  layout.drawY = layout.baselineY - metrics.top;
  layout.advanceX = std::max(1, advanceOverride >= 0 ? advanceOverride : static_cast<int>(metrics.advanceX));
  layout.trimLeadingEmptyColumns = false;
  return layout;
}
