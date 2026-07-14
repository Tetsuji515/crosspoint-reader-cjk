#pragma once

#include "../Activity.h"
#include "components/themes/BaseTheme.h"  // for Rect
#include "fontIds.h"
#include "util/ButtonNavigator.h"

class LauncherActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  // Single UI font size used for every glyph in the launcher (see the tracked-
  // text helpers in the .cpp). UI fonts are never rerouted to the external
  // reader font, so ASCII renders predictably at one consistent size.
  static constexpr int LAUNCHER_FONT_ID = UI_12_FONT_ID;

  // Height reserved at the top for the header: wordmark + battery row plus the
  // bezel with the month calendar. Just over 1/3 of the 800px portrait screen
  // so 12pt day digits get non-overlapping week rows; the rest is the list.
  static constexpr int HEADER_HEIGHT = 300;

  // Ghosting mitigation for list-area partial updates. See src/util/RefreshCycle.h.
  static constexpr int LIST_REFRESH_CYCLE_N = 10;
  int listMovesUntilFullRefresh = LIST_REFRESH_CYCLE_N;

  // "Coming soon" toast for not-yet-implemented apps.
  bool toastVisible = false;
  unsigned long toastShownAtMs = 0;
  static constexpr unsigned long TOAST_DURATION_MS = 1500;

  enum class RenderScope { Full, ListOnly };
  RenderScope pendingScope = RenderScope::Full;

  // Ignore button events until Back/Confirm are fully released after entering.
  // Apps like Settings/FileTransfer exit on Back *press*, so the matching Back
  // *release* would otherwise bleed through into this freshly-created launcher
  // and immediately trigger handleBack() (re-opening the last book), which was
  // the confirmed cause of a low-memory crash on home return. Mirrors the same
  // guard EpubReaderActivity uses. See docs/dev-notes/mem-investigation.md.
  bool skipNextButtonCheck = true;

  void handleConfirm();
  Rect computeListRect() const;
  void renderHeader() const;
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
