#include "ExternalGlyphLayout.h"

#include <SDCardManager.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

void appendBytes(std::vector<uint8_t>& bytes, std::initializer_list<uint8_t> values) {
  bytes.insert(bytes.end(), values.begin(), values.end());
}

void appendInt16(std::vector<uint8_t>& bytes, int16_t value) {
  bytes.push_back(static_cast<uint8_t>(value & 0xFF));
  bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendUint16(std::vector<uint8_t>& bytes, uint16_t value) {
  bytes.push_back(static_cast<uint8_t>(value & 0xFF));
  bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendUint32(std::vector<uint8_t>& bytes, uint32_t value) {
  bytes.push_back(static_cast<uint8_t>(value & 0xFF));
  bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  bytes.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  bytes.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

std::vector<uint8_t> makeXbf2FontWithOneGlyph() {
  std::vector<uint8_t> xbf2;
  appendBytes(xbf2, {'X', 'B', 'F', '2'});
  appendBytes(xbf2, {8, 12});
  appendInt16(xbf2, 9);
  appendInt16(xbf2, -3);
  appendUint16(xbf2, 14);
  appendUint32(xbf2, 20);
  appendUint32(xbf2, 32);
  appendUint32(xbf2, 0x41);
  appendInt16(xbf2, 2);
  appendInt16(xbf2, 11);
  appendUint16(xbf2, 6);
  appendUint16(xbf2, 0x01);
  for (int i = 0; i < 12; ++i) {
    xbf2.push_back(i == 0 ? 0x20 : 0x00);
  }
  return xbf2;
}

}  // namespace

int main() {
  const char* path = "/fonts/LayoutFont_16_8x12.bin";
  HostStorage::clear();
  HostStorage::registerFile(path, makeXbf2FontWithOneGlyph());

  ExternalFont font;
  expect(font.load(path), "Expected ExternalFont::load() to succeed for layout test");

  ExternalGlyphMetrics metrics{};
  expect(font.getGlyphMetrics(0x41, &metrics), "Expected external glyph metrics to load for layout test");

  const int cursorX = 30;
  const int baselineY = 59;
  const ExternalGlyphLayout layout = computeExternalGlyphLayout(cursorX, baselineY, font, metrics);

  expect(layout.baselineY == 59,
         "Expected external glyph layout to treat incoming y as baseline instead of adding ascender again");
  expect(layout.drawX == 32, "Expected drawX = cursorX + left");
  expect(layout.drawY == 48, "Expected drawY = baselineY - top");
  expect(layout.advanceX == 6, "Expected cursor advance to equal advanceX");
  expect(!layout.trimLeadingEmptyColumns,
         "Expected richer external metrics path to keep explicit left bearing instead of trimming minX");

  ExternalGlyphMetrics negativeBearingMetrics = metrics;
  negativeBearingMetrics.left = -4;
  negativeBearingMetrics.top = 7;
  negativeBearingMetrics.advanceX = 300;
  const ExternalGlyphLayout wideAdvanceLayout = computeExternalGlyphLayout(cursorX, baselineY, font, negativeBearingMetrics);
  expect(wideAdvanceLayout.drawX == 26, "Expected drawX to preserve negative left bearing");
  expect(wideAdvanceLayout.drawY == 52, "Expected drawY = baselineY - top for alternate metrics");
  expect(wideAdvanceLayout.advanceX == 300, "Expected layout advanceX to preserve full 16-bit glyph advance");

  return 0;
}
