#pragma once

#include <cstdint>

#include <ExternalFont.h>

int getExternalGlyphAdvanceForRendering(const ExternalFont& font, uint32_t codepoint, int spacing);
