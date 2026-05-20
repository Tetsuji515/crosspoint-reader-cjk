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

std::vector<uint8_t> makeLegacyFontWithCornerQuoteFallback() {
  constexpr uint8_t kCharWidth = 8;
  constexpr uint8_t kCharHeight = 12;
  constexpr uint8_t kBytesPerRow = 1;
  constexpr uint16_t kBytesPerChar = kBytesPerRow * kCharHeight;
  constexpr uint32_t kCornerOpeningQuote = 0x300C;

  std::vector<uint8_t> fontData((kCornerOpeningQuote + 1) * kBytesPerChar, 0x00);
  const size_t glyphOffset = static_cast<size_t>(kCornerOpeningQuote) * kBytesPerChar;

  for (uint8_t row = 2; row <= 9; ++row) {
    fontData[glyphOffset + row] = 0x80;
  }
  fontData[glyphOffset + 2] = 0xF0;

  return fontData;
}

std::vector<uint8_t> makeLegacyFontWithAsciiQuoteFallback() {
  constexpr uint8_t kCharWidth = 8;
  constexpr uint8_t kCharHeight = 12;
  constexpr uint8_t kBytesPerRow = 1;
  constexpr uint16_t kBytesPerChar = kBytesPerRow * kCharHeight;
  constexpr uint32_t kSmartOpeningQuote = 0x201C;
  constexpr uint32_t kAsciiQuote = 0x22;

  std::vector<uint8_t> fontData((kSmartOpeningQuote + 1) * kBytesPerChar, 0x00);
  const size_t glyphOffset = static_cast<size_t>(kAsciiQuote) * kBytesPerChar;

  for (uint8_t row = 1; row <= 5; ++row) {
    fontData[glyphOffset + row] = 0x90;
  }

  return fontData;
}

void appendU16(std::vector<uint8_t>& data, uint16_t value) {
  data.push_back(static_cast<uint8_t>(value & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendI16(std::vector<uint8_t>& data, int16_t value) { appendU16(data, static_cast<uint16_t>(value)); }

void appendU32(std::vector<uint8_t>& data, uint32_t value) {
  data.push_back(static_cast<uint8_t>(value & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendInterval(std::vector<uint8_t>& data, uint32_t start, uint32_t end, uint32_t glyphOffset) {
  appendU32(data, start);
  appendU32(data, end);
  appendU32(data, glyphOffset);
}

void appendGlyphEntry(std::vector<uint8_t>& data, uint8_t width, uint8_t height, uint8_t advanceX, int16_t left,
                      int16_t top, uint32_t dataLength, uint32_t dataOffset) {
  data.push_back(width);
  data.push_back(height);
  data.push_back(advanceX);
  data.push_back(0);
  appendI16(data, left);
  appendI16(data, top);
  appendU32(data, dataLength);
  appendU32(data, dataOffset);
}

std::vector<uint8_t> makeEpdFontWithBlankOpeningQuoteAndSpace() {
  constexpr uint32_t kIntervalsOffset = 32;
  constexpr uint32_t kGlyphsOffset = kIntervalsOffset + 2 * 12;
  constexpr uint32_t kBitmapOffset = kGlyphsOffset + 2 * 16;

  std::vector<uint8_t> data;
  data.reserve(kBitmapOffset + 6);

  data.insert(data.end(), {'E', 'P', 'D', 'F'});
  appendU16(data, 1);
  data.push_back(0);   // 1-bit
  data.push_back(0);   // reserved
  data.push_back(16);  // line height
  data.push_back(12);  // ascender
  data.push_back(static_cast<uint8_t>(-4));
  data.push_back(0);
  appendU32(data, 2);  // interval count
  appendU32(data, 2);  // glyph count
  appendU32(data, kIntervalsOffset);
  appendU32(data, kGlyphsOffset);
  appendU32(data, kBitmapOffset);

  appendInterval(data, 0x20, 0x20, 0);
  appendInterval(data, 0x201C, 0x201C, 1);

  appendGlyphEntry(data, 0, 0, 8, 0, 0, 0, 0);          // ASCII space, advance-only
  appendGlyphEntry(data, 6, 8, 8, 0, 8, 6, 0);          // Opening quote with blank bitmap
  data.insert(data.end(), 6, static_cast<uint8_t>(0));  // Empty quote bitmap
  return data;
}

std::vector<uint8_t> makeEpdFontWithCornerQuoteFallback() {
  constexpr uint32_t kIntervalsOffset = 32;
  constexpr uint32_t kGlyphsOffset = kIntervalsOffset + 12;
  constexpr uint32_t kBitmapOffset = kGlyphsOffset + 16;

  std::vector<uint8_t> data;
  data.reserve(kBitmapOffset + 8);

  data.insert(data.end(), {'E', 'P', 'D', 'F'});
  appendU16(data, 1);
  data.push_back(0);   // 1-bit
  data.push_back(0);   // reserved
  data.push_back(16);  // line height
  data.push_back(12);  // ascender
  data.push_back(static_cast<uint8_t>(-4));
  data.push_back(0);
  appendU32(data, 1);  // interval count
  appendU32(data, 1);  // glyph count
  appendU32(data, kIntervalsOffset);
  appendU32(data, kGlyphsOffset);
  appendU32(data, kBitmapOffset);

  appendInterval(data, 0x300C, 0x300C, 0);
  appendGlyphEntry(data, 8, 8, 8, 0, 8, 8, 0);
  data.insert(data.end(), {0xF0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80});
  return data;
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

  const char* cornerQuotePath = "/fonts/LegacyCornerQuote_16_8x12.bin";
  HostStorage::clear();
  HostStorage::registerFile(cornerQuotePath, makeLegacyFontWithCornerQuoteFallback());

  ExternalFont cornerQuoteFont;
  expect(cornerQuoteFont.load(cornerQuotePath), "Expected legacy corner quote fallback font to load");
  expect(cornerQuoteFont.getGlyph(0x201C) != nullptr, "Expected smart opening quote to fall back to CJK corner quote");

  ExternalGlyphMetrics cornerQuoteMetrics{};
  expect(cornerQuoteFont.getGlyphMetrics(0x201C, &cornerQuoteMetrics),
         "Expected cached metrics for smart opening quote corner fallback");
  expect(cornerQuoteMetrics.advanceX == 8, "Expected corner quote fallback to advance by one full legacy cell");

  const char* asciiQuotePath = "/fonts/LegacyAsciiQuote_16_8x12.bin";
  HostStorage::clear();
  HostStorage::registerFile(asciiQuotePath, makeLegacyFontWithAsciiQuoteFallback());

  ExternalFont asciiQuoteFont;
  expect(asciiQuoteFont.load(asciiQuotePath), "Expected legacy ASCII quote fallback font to load");
  expect(asciiQuoteFont.getGlyph(0x201C) != nullptr, "Expected smart opening quote to fall back to ASCII quote");

  ExternalGlyphMetrics asciiQuoteMetrics{};
  expect(asciiQuoteFont.getGlyphMetrics(0x201C, &asciiQuoteMetrics),
         "Expected cached metrics for smart opening quote ASCII fallback");
  expect(asciiQuoteMetrics.advanceX < 8, "Expected ASCII quote fallback to keep narrow advance");

  const char* epdPath = "/fonts/BlankQuote_16_16x16.epdf";
  HostStorage::clear();
  HostStorage::registerFile(epdPath, makeEpdFontWithBlankOpeningQuoteAndSpace());

  ExternalFont epdFont;
  expect(epdFont.load(epdPath), "Expected minimal EPDF font to load");
  expect(epdFont.getGlyph(' ') != nullptr, "Expected advance-only space to stay renderable in EPDF font");

  ExternalGlyphMetrics spaceMetrics{};
  expect(epdFont.getGlyphMetrics(' ', &spaceMetrics), "Expected cached metrics for EPDF space");
  expect(spaceMetrics.advanceX == 8, "Expected EPDF space to preserve advance-only width");

  expect(epdFont.getGlyph(0x201C) == nullptr, "Expected blank opening quote to fall back instead of rendering nothing");

  const char* epdCornerQuotePath = "/fonts/EpdCornerQuote_16_16x16.epdf";
  HostStorage::clear();
  HostStorage::registerFile(epdCornerQuotePath, makeEpdFontWithCornerQuoteFallback());

  ExternalFont epdCornerQuoteFont;
  expect(epdCornerQuoteFont.load(epdCornerQuotePath), "Expected EPDF corner quote fallback font to load");
  expect(epdCornerQuoteFont.getGlyph(0x201C) != nullptr,
         "Expected EPDF smart opening quote to fall back to CJK corner quote");

  ExternalGlyphMetrics epdCornerQuoteMetrics{};
  expect(epdCornerQuoteFont.getGlyphMetrics(0x201C, &epdCornerQuoteMetrics),
         "Expected cached EPDF metrics for smart opening quote fallback");
  expect(epdCornerQuoteMetrics.advanceX == 8, "Expected EPDF corner quote fallback to preserve rich advance");

  return 0;
}
