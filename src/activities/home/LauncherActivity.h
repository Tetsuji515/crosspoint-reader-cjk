#pragma once

#include "../Activity.h"
#include "components/themes/BaseTheme.h"  // for Rect
#include "util/ButtonNavigator.h"

class LauncherActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  // Ghosting mitigation for list-area partial updates. See src/util/RefreshCycle.h.
  static constexpr int LIST_REFRESH_CYCLE_N = 10;
  int listMovesUntilFullRefresh = LIST_REFRESH_CYCLE_N;

  // "Coming soon" toast for not-yet-implemented apps.
  bool toastVisible = false;
  unsigned long toastShownAtMs = 0;
  static constexpr unsigned long TOAST_DURATION_MS = 1500;

  enum class RenderScope { Full, ListOnly, ClockOnly };
  RenderScope pendingScope = RenderScope::Full;

  // Clock area: minute-boundary partial refresh. Time source is ClockSync
  // (src/util/ClockSync.h) -- the launcher never connects to WiFi itself; see
  // docs/dev-notes/clock-sync-survey.md (judgment Y).
  int lastRenderedMinute = -1;

  void handleConfirm();
  void handleBack();
  Rect computeClockRect() const;
  Rect computeListRect() const;
  void renderClockArea() const;
  void renderListArea() const;
  void renderToast() const;

 public:
  explicit LauncherActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Launcher", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
