#include "ExternalFontRenderMetrics.h"

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
