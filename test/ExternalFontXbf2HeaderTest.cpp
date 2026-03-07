#include "ExternalFont.h"

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <vector>

#include <SDCardManager.h>

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

}  // namespace

int main() {
  const char* path = "/fonts/TestFont_16_8x12.bin";

  std::vector<uint8_t> xbf2;
  appendBytes(xbf2, {'X', 'B', 'F', '2'});
  appendBytes(xbf2, {8, 12});
  appendInt16(xbf2, 9);
  appendInt16(xbf2, -3);
  appendUint16(xbf2, 14);
  appendUint32(xbf2, 28);
  appendUint32(xbf2, 40);
  appendBytes(xbf2, {0, 0, 0, 0, 0, 0, 0, 0});

  appendUint32(xbf2, 0x41);
  appendInt16(xbf2, 1);
  appendInt16(xbf2, 23);
  appendUint16(xbf2, 18);
  appendUint16(xbf2, 0x01);

  std::vector<uint8_t> glyphBytes(12, 0xAB);
  xbf2.insert(xbf2.end(), glyphBytes.begin(), glyphBytes.end());

  HostStorage::clear();
  HostStorage::registerFile(path, xbf2);

  ExternalFont font;
  expect(font.load(path), "Expected ExternalFont::load() to succeed for minimal XBF2 header");
  expect(font.isRichMetricsFormat(), "Expected XBF2 font to report rich metrics format");
  expect(font.getAscender() == 9, "Expected ascender parsed from XBF2 header");
  expect(font.getDescender() == -3, "Expected descender parsed from XBF2 header");
  expect(font.getLineHeight() == 14, "Expected lineHeight parsed from XBF2 header");

  ExternalGlyphMetrics metrics{};
  expect(font.getGlyphMetrics(0x41, &metrics), "Expected getGlyphMetrics(0x41, &metrics) to succeed for XBF2 font");
  expect(metrics.left == 1, "Expected XBF2 glyph metrics left to equal 1");
  expect(metrics.top == 23, "Expected XBF2 glyph metrics top to equal 23");
  expect(metrics.advanceX == 18, "Expected XBF2 glyph metrics advanceX to equal 18");
  expect((metrics.flags & 0x01) == 0x01, "Expected XBF2 glyph metrics flags to include hasInk bit");

  const uint8_t* glyph = font.getGlyph(0);
  expect(glyph != nullptr, "Expected getGlyph(0) to return glyph data for XBF2 font");
  expect(glyph[0] == 0xAB, "Expected getGlyph(0) to read bitmap bytes after XBF2 header and metrics table");

  std::vector<uint8_t> invalidXbf2;
  appendBytes(invalidXbf2, {'X', 'B', 'F', '2'});
  appendBytes(invalidXbf2, {8, 12});
  appendInt16(invalidXbf2, 9);
  appendInt16(invalidXbf2, -3);
  appendUint16(invalidXbf2, 0);
  appendUint32(invalidXbf2, 20);
  appendUint32(invalidXbf2, 20);
  HostStorage::registerFile("/fonts/InvalidTestFont_16_8x12.bin", invalidXbf2);

  ExternalFont invalidFont;
  expect(!invalidFont.load("/fonts/InvalidTestFont_16_8x12.bin"),
         "Expected ExternalFont::load() to reject XBF2 headers with lineHeight=0");

  return 0;
}
