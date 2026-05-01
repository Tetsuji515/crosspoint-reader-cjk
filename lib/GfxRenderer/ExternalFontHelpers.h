#pragma once

#include <cstdint>

#include <ExternalFont.h>

// --- Glyph layout ---

struct ExternalGlyphLayout {
  int drawX = 0;
  int drawY = 0;
  int advanceX = 1;
  int baselineY = 0;
  bool trimLeadingEmptyColumns = false;
};

ExternalGlyphLayout computeExternalGlyphLayout(int cursorX, int baselineY, const ExternalFont& font,
                                               const ExternalGlyphMetrics& metrics, int advanceOverride = -1);

// --- Metrics initialization ---

/// Initialise an ExternalGlyphMetrics struct with the font's default cell
/// dimensions and then load the per-glyph metrics for `codepoint`.
inline ExternalGlyphMetrics getDefaultMetrics(const ExternalFont& font, uint32_t codepoint) {
  ExternalGlyphMetrics m{};
  m.width = font.getCharWidth();
  m.height = font.getCharHeight();
  m.advanceX = font.getCharWidth();
  font.getGlyphMetrics(codepoint, &m);
  return m;
}

/// For non-rich (.bin) format fonts, the left bearing is a minX pixel-scan
/// value rather than a true typographic bearing. Zero it out and fold it
/// into the advance so glyphs start at the cursor position.
/// Returns the adjusted advance width.
inline int adjustNonRichAdvance(ExternalGlyphMetrics& metrics, const ExternalFont& font) {
  int advance = metrics.advanceX;
  if (!font.isRichMetricsFormat()) {
    advance += metrics.left;
    metrics.left = 0;
  }
  return advance;
}

// --- Advance helpers ---

int clampExternalAdvance(int baseWidth, int spacing);

int getExternalGlyphAdvanceForRendering(const ExternalFont& font, uint32_t codepoint, int spacing);

// --- Font-level render metrics ---

int getExternalFontAscenderForRendering(const ExternalFont& font);
int getExternalFontLineHeightForRendering(const ExternalFont& font);
