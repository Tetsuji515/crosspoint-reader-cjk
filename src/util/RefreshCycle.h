#pragma once

#include <GfxRenderer.h>

// Small, generic ghosting-mitigation counter: after `cycleLength` partial-refresh
// (FAST_REFRESH) redraws, forces one full-screen HALF_REFRESH to clear residual
// e-ink ghosting, then resets. Deliberately independent from
// src/activities/reader/ReaderUtils.h's displayWithRefreshCycle(), which is tied to
// the user-configurable SETTINGS.getRefreshFrequency() reader setting; this helper
// uses a fixed, caller-owned counter instead so callers outside the reader don't
// need to depend on reader-only headers/settings.
inline void displayWithFixedRefreshCycle(const GfxRenderer& renderer, int& movesUntilFullRefresh,
                                         int cycleLength) {
  if (movesUntilFullRefresh <= 1) {
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
    movesUntilFullRefresh = cycleLength;
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    movesUntilFullRefresh--;
  }
}
