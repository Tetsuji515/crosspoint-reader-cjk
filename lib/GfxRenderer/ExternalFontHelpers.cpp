#include "ExternalFontHelpers.h"

#include <algorithm>

// --- Glyph layout ---

ExternalGlyphLayout computeExternalGlyphLayout(int cursorX, int baselineY, const ExternalFont& font,
                                               const ExternalGlyphMetrics& metrics, int advanceOverride) {
  ExternalGlyphLayout layout{};
  layout.baselineY = baselineY;
  layout.drawX = cursorX + metrics.left;
  layout.drawY = layout.baselineY - metrics.top;
  layout.advanceX = std::max(1, advanceOverride >= 0 ? advanceOverride : static_cast<int>(metrics.advanceX));
  layout.trimLeadingEmptyColumns = false;
  return layout;
}

// --- Advance helpers ---

int clampExternalAdvance(const int baseWidth, const int spacing) { return std::max(1, baseWidth + spacing); }

int getExternalGlyphAdvanceForRendering(const ExternalFont& font, uint32_t codepoint, int spacing) {
  ExternalGlyphMetrics metrics{};
  metrics.width = font.getCharWidth();
  metrics.height = font.getCharHeight();
  metrics.advanceX = font.getCharWidth();

  if (font.getGlyphMetrics(codepoint, &metrics)) {
    return clampExternalAdvance(metrics.advanceX, spacing);
  }

  return clampExternalAdvance(font.getCharWidth(), spacing);
}

// --- Font-level render metrics ---

int getExternalFontAscenderForRendering(const ExternalFont& font) {
  if (font.isRichMetricsFormat() && font.getAscender() > 0) {
    return font.getAscender();
  }
  return font.getCharHeight();
}

int getExternalFontLineHeightForRendering(const ExternalFont& font) {
  if (font.isRichMetricsFormat() && font.getLineHeight() > 0) {
    return font.getLineHeight();
  }
  return font.getCharHeight();
}
