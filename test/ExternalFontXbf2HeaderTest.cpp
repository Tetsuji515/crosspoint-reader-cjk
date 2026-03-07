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

}  // namespace

int main() {
  const char* path = "/fonts/TestFont_16_8x12.bin";

  std::vector<uint8_t> xbf2;
  appendBytes(xbf2, {'X', 'B', 'F', '2'});
  appendBytes(xbf2, {8, 12});
  appendInt16(xbf2, 9);
  appendInt16(xbf2, -3);
  appendUint16(xbf2, 14);

  HostStorage::clear();
  HostStorage::registerFile(path, xbf2);

  ExternalFont font;
  expect(font.load(path), "Expected ExternalFont::load() to succeed for minimal XBF2 header");
  expect(font.isRichMetricsFormat(), "Expected XBF2 font to report rich metrics format");
  expect(font.getAscender() == 9, "Expected ascender parsed from XBF2 header");
  expect(font.getDescender() == -3, "Expected descender parsed from XBF2 header");
  expect(font.getLineHeight() == 14, "Expected lineHeight parsed from XBF2 header");

  return 0;
}
