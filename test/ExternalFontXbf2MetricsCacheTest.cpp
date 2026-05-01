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

std::vector<uint8_t> makeXbf2Font() {
  constexpr uint32_t kCodepoint = 0x41;
  constexpr size_t kGlyphBytes = 12;
  constexpr size_t kBmpSlotCount = 0x10000;

  std::vector<uint8_t> xbf2;
  appendBytes(xbf2, {'X', 'B', 'F', '2'});
  appendBytes(xbf2, {8, 12});
  appendInt16(xbf2, 9);
  appendInt16(xbf2, -3);
  appendUint16(xbf2, 14);
  appendUint32(xbf2, 20);
  appendUint32(xbf2, 32);
  appendUint32(xbf2, kCodepoint);
  appendInt16(xbf2, 1);
  appendInt16(xbf2, 11);
  appendUint16(xbf2, 6);
  appendUint16(xbf2, 0x01);

  std::vector<uint8_t> bitmapData(kBmpSlotCount * kGlyphBytes, 0x00);
  bitmapData[(kCodepoint * kGlyphBytes)] = 0x20;
  xbf2.insert(xbf2.end(), bitmapData.begin(), bitmapData.end());
  return xbf2;
}

}  // namespace

int main() {
  const char* path = "/fonts/MetricsCacheFont_16_8x12.xbf2";
  HostStorage::clear();
  HostStorage::registerFile(path, makeXbf2Font());

  ExternalFont font;
  expect(font.load(path), "Expected XBF2 font to load for metrics cache test");

  HostStorage::resetIoCounters();
  const uint8_t* glyph = font.getGlyph(0x41);
  expect(glyph != nullptr, "Expected getGlyph() to populate cache for xbf2 metrics cache test");
  const size_t readsAfterGlyph = HostStorage::getReadCount();
  const size_t seeksAfterGlyph = HostStorage::getSeekCount();

  ExternalGlyphMetrics metrics{};
  expect(font.getGlyphMetrics(0x41, &metrics), "Expected getGlyphMetrics() to succeed after getGlyph() cache warmup");
  expect(metrics.advanceX == 6, "Expected cached metrics advanceX to match XBF2 metrics entry");
  expect(HostStorage::getReadCount() == readsAfterGlyph,
         "Expected getGlyphMetrics() after getGlyph() cache warmup to avoid extra file reads for XBF2 metrics");
  expect(HostStorage::getSeekCount() == seeksAfterGlyph,
         "Expected getGlyphMetrics() after getGlyph() cache warmup to avoid extra file seeks for XBF2 metrics");

  return 0;
}
