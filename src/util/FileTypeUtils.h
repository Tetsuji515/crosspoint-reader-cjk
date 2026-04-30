#pragma once

#include <string>

namespace FileTypeUtils {

enum class FileOpenRoute {
  Unsupported,
  EpubReader,
  XtcReader,
  TxtReader,
  ImageViewer,
  FontPreview,
};

bool isBookFile(const std::string& path);
bool isImageFile(const std::string& path);
bool isDirectlyViewableImageFile(const std::string& path);
bool isFontPreviewFile(const std::string& path);
bool isVisibleInFileBrowser(const std::string& path);
FileOpenRoute getOpenRoute(const std::string& path);

}  // namespace FileTypeUtils
