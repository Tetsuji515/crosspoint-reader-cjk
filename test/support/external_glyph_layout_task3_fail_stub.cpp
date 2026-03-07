#include "ExternalGlyphLayout.h"

ExternalGlyphLayout computeExternalGlyphLayout(int cursorX, int lineTopY, const ExternalFont& font,
                                               const ExternalGlyphMetrics& metrics, int advanceOverride) {
  ExternalGlyphLayout layout{};
  layout.baselineY = lineTopY + font.getCharHeight();
  layout.drawX = cursorX;
  layout.drawY = layout.baselineY - metrics.height;
  layout.advanceX = advanceOverride >= 0 ? advanceOverride : font.getCharWidth();
  layout.trimLeadingEmptyColumns = true;
  return layout;
}
