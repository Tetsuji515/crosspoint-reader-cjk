#include "ExternalFontLabel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
std::string getFontFormatLabel(const char* filename) {
  const char* ext = strrchr(filename, '.');
  if (!ext || *(ext + 1) == '\0') {
    return "?";
  }

  std::string format = ext + 1;
  std::transform(format.begin(), format.end(), format.begin(),
                 [](const unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return format;
}
}  // namespace

std::string buildExternalFontLabel(const char* filename, const char* fontName, uint8_t size, bool loadable) {
  const std::string format = getFontFormatLabel(filename);
  char label[96];
  snprintf(label, sizeof(label), "%s(%dpt)[%s]%s", fontName, size, format.c_str(), loadable ? "" : " [!]");
  return std::string(label);
}
