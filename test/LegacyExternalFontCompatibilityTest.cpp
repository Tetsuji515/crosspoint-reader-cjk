#include <SDCardManager.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "ExternalFont.h"

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

std::vector<uint8_t> makeLegacyFontWithAsciiExclamation() {
  constexpr uint8_t kCharWidth = 8;
  constexpr uint8_t kCharHeight = 12;
  constexpr uint8_t kBytesPerRow = 1;
  constexpr uint16_t kBytesPerChar = kBytesPerRow * kCharHeight;
  constexpr uint32_t kAsciiExclamation = 0x21;

  std::vector<uint8_t> fontData((kAsciiExclamation + 1) * kBytesPerChar, 0x00);
  const size_t glyphOffset = static_cast<size_t>(kAsciiExclamation) * kBytesPerChar;

  for (uint8_t row = 1; row <= 8; ++row) {
    fontData[glyphOffset + row] = 0x20;
  }
  fontData[glyphOffset + 10] = 0x20;

  return fontData;
}

}  // namespace

int main() {
  const char* path = "/fonts/LegacyAscii_16_8x12.bin";
  HostStorage::clear();
  HostStorage::registerFile(path, makeLegacyFontWithAsciiExclamation());

  ExternalFont font;
  expect(font.load(path), "Expected legacy external font to load for fallback compatibility test");

  const uint8_t* glyph = font.getGlyph(0xFF01);
  expect(glyph != nullptr, "Expected fullwidth exclamation to fall back to ASCII glyph data");

  ExternalGlyphMetrics metrics{};
  expect(font.getGlyphMetrics(0xFF01, &metrics), "Expected cached metrics for fullwidth exclamation fallback glyph");
  expect(metrics.left == 2, "Expected fallback glyph metrics to preserve ASCII left bearing");
  expect(metrics.advanceX == 3,
         "Expected fullwidth exclamation fallback to use ASCII narrow advance instead of fullwidth charWidth");
  expect((metrics.flags & 0x01) == 0x01, "Expected fallback glyph metrics to mark glyph as having ink");

  return 0;
}
