#pragma once

#include <cstdint>
#include <string>

#include "ReaderRuntimePolicy.h"

namespace ReaderRuntime {

enum class ReaderOperation : uint8_t {
  None,
  OpenSection,
  BuildSection,
  LoadPage,
  PreloadGlyphs,
  RenderPage,
  DisplayRefresh,
  GrayscalePass,
};

struct ReaderOperationTrace {
  uint32_t magic = 0;
  uint8_t version = 1;
  ReaderKind readerKind = ReaderKind::Epub;
  ReaderOperation operation = ReaderOperation::None;
  int16_t sectionIndex = -1;
  int16_t pageIndex = -1;
  RefreshMode refreshMode = RefreshMode::Fast;
  RefreshReason refreshReason = RefreshReason::NormalText;
  uint32_t freeHeap = 0;
  uint32_t minFreeHeap = 0;
  uint16_t loadMs = 0;
  uint16_t preloadMs = 0;
  uint16_t renderMs = 0;
  uint16_t displayMs = 0;
  uint16_t grayscaleMs = 0;
  uint16_t totalMs = 0;
};

void clearLastReaderOperationTrace();
void setLastReaderOperationTrace(const ReaderOperationTrace& trace);
bool hasLastReaderOperationTrace();
ReaderOperationTrace getLastReaderOperationTrace();
std::string formatLastReaderOperationTrace();

}  // namespace ReaderRuntime
