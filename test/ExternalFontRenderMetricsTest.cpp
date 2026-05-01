#include <SDCardManager.h>

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <vector>

#include "ExternalFontHelpers.h"

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
  const char* path = "/fonts/RenderMetricsFont_16_8x12.bin";
  HostStorage::clear();
  HostStorage::registerFile(path, makeXbf2FontWithOneGlyph());

  ExternalFont font;
  expect(font.load(path), "Expected ExternalFont::load() to succeed for render metrics test");
  expect(font.isRichMetricsFormat(), "Expected XBF2 test font to expose rich metrics format");
  expect(getExternalFontAscenderForRendering(font) == 9,
         "Expected rendering ascender to come from XBF2 font ascender instead of charHeight");
  expect(getExternalFontLineHeightForRendering(font) == 14,
         "Expected rendering lineHeight to come from XBF2 font lineHeight instead of charHeight");

  return 0;
}
