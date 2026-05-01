#include <SDCardManager.h>

#include <cstdint>
#include <cstdlib>
#include <initializer_list>
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

std::vector<uint8_t> makeSortedXbf2Font() {
  constexpr size_t kGlyphBytes = 12;
  constexpr size_t kBmpSlotCount = 0x10000;
  constexpr uint32_t kCodepoints[] = {0x20, 0x30, 0x41, 0x42, 0x43, 0x50, 0x60, 0x70,
                                      0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0};

  std::vector<uint8_t> xbf2;
  appendBytes(xbf2, {'X', 'B', 'F', '2'});
  appendBytes(xbf2, {8, 12});
  appendInt16(xbf2, 9);
  appendInt16(xbf2, -3);
  appendUint16(xbf2, 14);
  appendUint32(xbf2, 20);
  appendUint32(xbf2, 20 + sizeof(kCodepoints) / sizeof(kCodepoints[0]) * 12);

  for (size_t index = 0; index < sizeof(kCodepoints) / sizeof(kCodepoints[0]); ++index) {
    appendUint32(xbf2, kCodepoints[index]);
    appendInt16(xbf2, static_cast<int16_t>(index));
    appendInt16(xbf2, static_cast<int16_t>(20 - index));
    appendUint16(xbf2, static_cast<uint16_t>(100 + index));
    appendUint16(xbf2, 0x01);
  }

  std::vector<uint8_t> bitmapData(kBmpSlotCount * kGlyphBytes, 0x00);
  bitmapData[(0x70 * kGlyphBytes)] = 0x5A;
  xbf2.insert(xbf2.end(), bitmapData.begin(), bitmapData.end());
  return xbf2;
}

}  // namespace

int main() {
  const char* path = "/fonts/BinarySearchFont_16_8x12.xbf2";
  HostStorage::clear();
  HostStorage::registerFile(path, makeSortedXbf2Font());

  ExternalFont font;
  expect(font.load(path), "Expected sorted XBF2 font to load for binary search test");

  HostStorage::resetIoCounters();
  ExternalGlyphMetrics metrics{};
  expect(font.getGlyphMetrics(0x70, &metrics),
         "Expected getGlyphMetrics() to find middle metrics entry in sorted XBF2 table");
  expect(metrics.left == 7, "Expected binary search test metrics left to match sorted entry");
  expect(metrics.top == 13, "Expected binary search test metrics top to match sorted entry");
  expect(metrics.advanceX == 107, "Expected binary search test metrics advanceX to match sorted entry");
  expect(HostStorage::getSeekCount() <= 4,
         "Expected sorted XBF2 metrics lookup to use logarithmic seek count instead of near-linear scan");
  expect(HostStorage::getReadCount() <= 4,
         "Expected sorted XBF2 metrics lookup to use logarithmic read count instead of near-linear scan");

  return 0;
}
