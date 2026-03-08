#pragma once

#include <ExternalFont.h>

struct ExternalGlyphLayout {
  int drawX = 0;
  int drawY = 0;
  int advanceX = 1;
  int baselineY = 0;
  bool trimLeadingEmptyColumns = false;
};

ExternalGlyphLayout computeExternalGlyphLayout(int cursorX, int baselineY, const ExternalFont& font,
                                               const ExternalGlyphMetrics& metrics, int advanceOverride = -1);
