#include <cstdlib>
#include <iostream>
#include <string>

#include "util/ExternalFontLabel.h"

namespace {
void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    std::exit(1);
  }
}
}  // namespace

int main() {
  expect(buildExternalFontLabel("MyFont_16_8x12.xbf2", "MyFont", 16, true) == "MyFont(16pt)[XBF2]",
         "Expected XBF2 font label to include uppercase format suffix in brackets");
  expect(buildExternalFontLabel("OtherFont_16_8x12.bin", "OtherFont", 16, true) == "OtherFont(16pt)[BIN]",
         "Expected legacy bin font label to include BIN format suffix in brackets");
  expect(buildExternalFontLabel("OtherFont_16_8x12.bin", "OtherFont", 16, false) == "OtherFont(16pt)[BIN] [!]",
         "Expected non-loadable font label to keep warning marker after format suffix");
  std::cout << "EXTERNAL_FONT_LABEL_FORMAT_OK" << std::endl;
  return 0;
}
