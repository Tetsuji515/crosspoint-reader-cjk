#include "ExternalFont.h"

#include <SDCardManager.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

std::vector<uint8_t> readFixtureBytes(const char* relativePath) {
  std::ifstream input(relativePath, std::ios::binary);
  expect(input.good(), "Expected committed XBF2 fixture file to exist");
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  const char* fixturePath = "test/fixtures/maker-task7-TestFont_28_33x39.bin";
  const char* mountedPath = "/fonts/TestFont_28_33x39.bin";

  HostStorage::clear();
  HostStorage::registerFile(mountedPath, readFixtureBytes(fixturePath));

  ExternalFont font;
  expect(font.load(mountedPath), "Expected ExternalFont::load() to succeed for maker-generated XBF2 fixture");
  expect(font.isRichMetricsFormat(), "Expected maker-generated fixture to load as rich metrics format");
  expect(font.getAscender() == 25, "Expected ascender parsed from maker-generated fixture");
  expect(font.getDescender() == -3, "Expected descender parsed from maker-generated fixture");
  expect(font.getLineHeight() == 28, "Expected lineHeight parsed from maker-generated fixture");

  ExternalGlyphMetrics metricsA{};
  expect(font.getGlyphMetrics(0x41, &metricsA), "Expected A metrics in maker-generated fixture");
  expect(metricsA.left == 3, "Expected A.left from maker-generated fixture");
  expect(metricsA.top == 22, "Expected A.top from maker-generated fixture");
  expect(metricsA.advanceX == 28, "Expected A.advanceX from maker-generated fixture");
  expect((metricsA.flags & 0x01) == 0x01, "Expected A flags to include hasInk");

  ExternalGlyphMetrics metricsG{};
  expect(font.getGlyphMetrics(0x67, &metricsG), "Expected g metrics in maker-generated fixture");
  expect(metricsG.left == 4, "Expected g.left from maker-generated fixture");
  expect(metricsG.top == 14, "Expected g.top from maker-generated fixture");
  expect(metricsG.advanceX == 25, "Expected g.advanceX from maker-generated fixture");
  expect((metricsG.flags & 0x01) == 0x01, "Expected g flags to include hasInk");

  ExternalGlyphMetrics metricsSpace{};
  expect(font.getGlyphMetrics(0x20, &metricsSpace), "Expected space metrics in maker-generated fixture");
  expect(metricsSpace.left == 0, "Expected space.left from maker-generated fixture");
  expect(metricsSpace.top == 0, "Expected space.top from maker-generated fixture");
  expect(metricsSpace.advanceX == 14, "Expected space.advanceX from maker-generated fixture");
  expect(metricsSpace.flags == 0, "Expected space.flags to remain zero without ink");

  return 0;
}
