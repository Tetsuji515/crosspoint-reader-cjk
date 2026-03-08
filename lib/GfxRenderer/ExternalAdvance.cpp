#include "ExternalAdvance.h"

#include <algorithm>

namespace {

int clampExternalAdvance(const int baseWidth, const int spacing) { return std::max(1, baseWidth + spacing); }

}  // namespace

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
