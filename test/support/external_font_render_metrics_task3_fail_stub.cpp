#include "ExternalFontRenderMetrics.h"

int getExternalFontAscenderForRendering(const ExternalFont& font) { return font.getCharHeight(); }

int getExternalFontLineHeightForRendering(const ExternalFont& font) { return font.getCharHeight(); }
