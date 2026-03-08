#include "ExternalFontLabel.h"

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
  for (char& ch : format) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  }
  return format;
}
}  // namespace

std::string buildExternalFontLabel(const char* filename, const char* fontName, uint8_t size, bool loadable) {
  const std::string format = getFontFormatLabel(filename);
  char label[96];
  snprintf(label, sizeof(label), "%s(%dpt)[%s]%s", fontName, size, format.c_str(), loadable ? "" : " [!]");
  return std::string(label);
}
