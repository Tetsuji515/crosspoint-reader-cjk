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

  // Clock area: minute-boundary partial refresh.
  int lastRenderedMinute = -1;

  // Silent NTP sync (A-6): only attempted if the clock looks unsynced and a
  // previously-saved WiFi network is available. No FreeRTOS task -- follows the
  // same fire-and-poll pattern as WifiSelectionActivity (WiFi.begin() then poll
  // WiFi.status() from loop()).
  enum class NtpSyncState { Idle, Connecting, Done, Failed };
  NtpSyncState ntpState = NtpSyncState::Idle;
  unsigned long wifiAttemptStartMs = 0;
  static constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 9000;

  void handleConfirm();
  void handleBack();
  void maybeStartSilentNtpSync();
  void pollSilentNtpSync();
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
