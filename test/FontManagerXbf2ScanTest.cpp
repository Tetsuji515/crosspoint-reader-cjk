#include "FontManager.h"

#include <SDCardManager.h>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}

std::vector<uint8_t> makeMinimalFontFile() { return std::vector<uint8_t>(12, 0x00); }

}  // namespace

int main() {
  HostStorage::clear();
  HostStorage::registerFile("/fonts/VisibleLegacy_16_8x12.bin", makeMinimalFontFile());
  HostStorage::registerFile("/fonts/VisibleRich_16_8x12.xbf2", makeMinimalFontFile());

  FontManager& manager = FontMgr;
  manager.scanFonts();

  expect(manager.getFontCount() == 2,
         "Expected FontManager::scanFonts() to include both .bin and .xbf2 font files in the font list");

  bool sawLegacyBin = false;
  bool sawRichXbf2 = false;
  for (int i = 0; i < manager.getFontCount(); ++i) {
    const FontInfo* info = manager.getFontInfo(i);
    expect(info != nullptr, "Expected scanned font info entry to exist");
    if (std::string(info->filename) == "VisibleLegacy_16_8x12.bin") {
      sawLegacyBin = true;
    }
    if (std::string(info->filename) == "VisibleRich_16_8x12.xbf2") {
      sawRichXbf2 = true;
    }
  }
  expect(sawLegacyBin, "Expected legacy .bin font to remain visible in scan results");
  expect(sawRichXbf2, "Expected .xbf2 font to be visible in scan results");

  return 0;
}
