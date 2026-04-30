#include "FileTypeUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace FileTypeUtils {
namespace {

bool hasAnyExtension(const std::string& path, std::string_view const* extensions, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const std::string_view extension = extensions[i];
    if (path.length() < extension.length()) {
      continue;
    }

    const size_t start = path.length() - extension.length();
    bool matches = true;
    for (size_t j = 0; j < extension.length(); ++j) {
      const unsigned char lhs = static_cast<unsigned char>(path[start + j]);
      const unsigned char rhs = static_cast<unsigned char>(extension[j]);
      if (std::tolower(lhs) != std::tolower(rhs)) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return true;
    }
  }

  return false;
}

constexpr std::array<std::string_view, 5> kBookExtensions = {
    ".epub",
    ".xtc",
    ".xtch",
    ".txt",
    ".md",
};

constexpr std::array<std::string_view, 4> kViewableImageExtensions = {
    ".bmp",
    ".jpg",
    ".jpeg",
    ".png",
};

constexpr std::array<std::string_view, 2> kFontExtensions = {
    ".bin",
    ".xbf2",
};

}  // namespace

bool isBookFile(const std::string& path) { return hasAnyExtension(path, kBookExtensions.data(), kBookExtensions.size()); }

bool isImageFile(const std::string& path) {
  return hasAnyExtension(path, kViewableImageExtensions.data(), kViewableImageExtensions.size());
}

bool isDirectlyViewableImageFile(const std::string& path) { return isImageFile(path); }

bool isFontPreviewFile(const std::string& path) {
  return hasAnyExtension(path, kFontExtensions.data(), kFontExtensions.size());
}

bool isVisibleInFileBrowser(const std::string& path) {
  return isBookFile(path) || isImageFile(path) || isFontPreviewFile(path);
}

FileOpenRoute getOpenRoute(const std::string& path) {
  if (hasAnyExtension(path, std::array<std::string_view, 1>{".epub"}.data(), 1)) {
    return FileOpenRoute::EpubReader;
  }
  if (hasAnyExtension(path, std::array<std::string_view, 2>{".xtc", ".xtch"}.data(), 2)) {
    return FileOpenRoute::XtcReader;
  }
  if (hasAnyExtension(path, std::array<std::string_view, 2>{".txt", ".md"}.data(), 2)) {
    return FileOpenRoute::TxtReader;
  }
  if (isDirectlyViewableImageFile(path)) {
    return FileOpenRoute::ImageViewer;
  }
  if (isFontPreviewFile(path)) {
    return FileOpenRoute::FontPreview;
  }
  return FileOpenRoute::Unsupported;
}

}  // namespace FileTypeUtils
