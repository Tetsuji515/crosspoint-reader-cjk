#pragma once

#include <cstdint>

namespace ReaderRuntime {

enum class ReaderKind : uint8_t {
  Epub,
  Txt,
  Xtc,
};

enum class MemoryDecision : uint8_t {
  Proceed,
  Degrade,
  Stop,
};

enum class RefreshMode : uint8_t {
  Fast,
  DarkRedrive,
  Half,
};

enum class RefreshReason : uint8_t {
  NormalText,
  DarkMode,
  Cadence,
  ImageDoubleFast,
  LowMemoryDegrade,
  ChapterBoundary,
  StateResync,
  ContinuousTurns,
};

struct MemoryThresholds {
  static constexpr uint32_t sectionBuildStopHeap = 50U * 1024U;
  static constexpr uint32_t optionalQualityHeap = 80U * 1024U;
};

struct RefreshContext {
  ReaderKind readerKind = ReaderKind::Epub;
  bool darkMode = false;
  bool containsImages = false;
  bool textAntiAliasing = false;
  bool externalFontEnabled = false;
  bool grayscaleRequested = false;
  bool chapterBoundary = false;
  bool settingsChanged = false;
  bool wakeResume = false;
  bool cacheRebuilt = false;
  bool lowMemory = false;
  uint16_t cadenceRemaining = 0;
  uint16_t refreshFrequency = 1;
  uint8_t consecutiveTurns = 0;
};

struct RefreshDecision {
  RefreshMode mode = RefreshMode::Fast;
  RefreshReason reason = RefreshReason::NormalText;
  bool useImageDoubleFast = false;
  bool runGrayscalePass = false;
  uint16_t nextCadenceRemaining = 1;
};

MemoryDecision classifyReaderMemory(uint32_t freeHeap, uint32_t requiredHeap = 0);
RefreshDecision chooseReaderRefresh(const RefreshContext& context);

}  // namespace ReaderRuntime
