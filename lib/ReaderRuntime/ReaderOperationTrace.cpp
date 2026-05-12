#include "ReaderOperationTrace.h"

#include <cstdio>

#if defined(ARDUINO)
#include "esp_private/esp_system_attr.h"
#define READER_TRACE_RTC_ATTR RTC_NOINIT_ATTR
#else
#define READER_TRACE_RTC_ATTR
#endif

namespace ReaderRuntime {
namespace {

constexpr uint32_t TRACE_MAGIC = 0x43505254;  // CPRT

READER_TRACE_RTC_ATTR ReaderOperationTrace lastTrace;

const char* readerKindName(const ReaderKind kind) {
  switch (kind) {
    case ReaderKind::Epub:
      return "EPUB";
    case ReaderKind::Txt:
      return "TXT";
    case ReaderKind::Xtc:
      return "XTC";
  }
  return "unknown";
}

const char* operationName(const ReaderOperation operation) {
  switch (operation) {
    case ReaderOperation::None:
      return "none";
    case ReaderOperation::OpenSection:
      return "open_section";
    case ReaderOperation::BuildSection:
      return "build_section";
    case ReaderOperation::LoadPage:
      return "load_page";
    case ReaderOperation::PreloadGlyphs:
      return "preload_glyphs";
    case ReaderOperation::RenderPage:
      return "render_page";
    case ReaderOperation::DisplayRefresh:
      return "display_refresh";
    case ReaderOperation::GrayscalePass:
      return "grayscale_pass";
  }
  return "unknown";
}

const char* refreshModeName(const RefreshMode mode) {
  switch (mode) {
    case RefreshMode::Fast:
      return "fast";
    case RefreshMode::DarkRedrive:
      return "dark_redrive";
    case RefreshMode::Half:
      return "half";
  }
  return "unknown";
}

const char* refreshReasonName(const RefreshReason reason) {
  switch (reason) {
    case RefreshReason::NormalText:
      return "normal_text";
    case RefreshReason::DarkMode:
      return "dark_mode";
    case RefreshReason::Cadence:
      return "cadence";
    case RefreshReason::ImageDoubleFast:
      return "image_double_fast";
    case RefreshReason::LowMemoryDegrade:
      return "low_memory_degrade";
    case RefreshReason::ChapterBoundary:
      return "chapter_boundary";
    case RefreshReason::StateResync:
      return "state_resync";
    case RefreshReason::ContinuousTurns:
      return "continuous_turns";
  }
  return "unknown";
}

}  // namespace

void clearLastReaderOperationTrace() { lastTrace = ReaderOperationTrace{}; }

void setLastReaderOperationTrace(const ReaderOperationTrace& trace) {
  lastTrace = trace;
  lastTrace.magic = TRACE_MAGIC;
}

bool hasLastReaderOperationTrace() { return lastTrace.magic == TRACE_MAGIC; }

ReaderOperationTrace getLastReaderOperationTrace() { return lastTrace; }

std::string formatLastReaderOperationTrace() {
  if (!hasLastReaderOperationTrace()) {
    return "none";
  }

  char buffer[384];
  std::snprintf(buffer, sizeof(buffer),
                "reader=%s\noperation=%s\nsection=%d\npage=%d\nrefresh=%s\nreason=%s\nfree_heap=%lu\n"
                "min_free_heap=%lu\nload_ms=%u\npreload_ms=%u\nrender_ms=%u\ndisplay_ms=%u\ngrayscale_ms=%u\n"
                "total_ms=%u",
                readerKindName(lastTrace.readerKind), operationName(lastTrace.operation), lastTrace.sectionIndex,
                lastTrace.pageIndex, refreshModeName(lastTrace.refreshMode), refreshReasonName(lastTrace.refreshReason),
                static_cast<unsigned long>(lastTrace.freeHeap), static_cast<unsigned long>(lastTrace.minFreeHeap),
                lastTrace.loadMs, lastTrace.preloadMs, lastTrace.renderMs, lastTrace.displayMs, lastTrace.grayscaleMs,
                lastTrace.totalMs);
  return buffer;
}

}  // namespace ReaderRuntime
