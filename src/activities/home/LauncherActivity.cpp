#include "LauncherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>

#include "AppRegistry.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/ClockSync.h"
#include "util/RefreshCycle.h"

namespace {
// Returns the first UTF-8 character of `s` as its own string (icon placeholder
// initial letter). Byte-length aware so CJK app names don't get mangled.
std::string firstUtf8Char(const char* s) {
  if (!s || !s[0]) return "";
  const auto c = static_cast<unsigned char>(s[0]);
  size_t len = 1;
  if ((c & 0xE0) == 0xC0) {
    len = 2;
  } else if ((c & 0xF0) == 0xE0) {
    len = 3;
  } else if ((c & 0xF8) == 0xF0) {
    len = 4;
  }
  return std::string(s, len);
}

int wrapIndex(int idx, int count) {
  idx %= count;
  if (idx < 0) idx += count;
  return idx;
}
}  // namespace

void LauncherActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  listMovesUntilFullRefresh = LIST_REFRESH_CYCLE_N;
  lastRenderedMinute = -1;
  pendingScope = RenderScope::Full;
  skipNextButtonCheck = true;
  requestUpdate();
}

void LauncherActivity::onExit() { Activity::onExit(); }

void LauncherActivity::loop() {
  // Skip button processing until a Back/Confirm press-release that started
  // before this launcher was entered has fully cleared. Prevents an app's
  // exit-on-Back-press from bleeding its Back release into handleBack() here
  // and re-opening the last book. Mirrors EpubReaderActivity::loop().
  if (skipNextButtonCheck) {
    const bool confirmCleared = !mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
                                !mappedInput.wasReleased(MappedInputManager::Button::Confirm);
    const bool backCleared = !mappedInput.isPressed(MappedInputManager::Button::Back) &&
                             !mappedInput.wasReleased(MappedInputManager::Button::Back);
    if (confirmCleared && backCleared) {
      skipNextButtonCheck = false;
    }
    return;
  }

  buttonNavigator.onNext([this] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, APP_REGISTRY_COUNT);
    pendingScope = RenderScope::ListOnly;
    requestUpdate();
  });

  buttonNavigator.onPrevious([this] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, APP_REGISTRY_COUNT);
    pendingScope = RenderScope::ListOnly;
    requestUpdate();
  });

  if (toastVisible && millis() - toastShownAtMs >= TOAST_DURATION_MS) {
    toastVisible = false;
    pendingScope = RenderScope::Full;
    requestUpdate();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    handleConfirm();
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    handleBack();
  }

  struct tm tmNow{};
  const time_t now = time(nullptr);
  localtime_r(&now, &tmNow);
  if (tmNow.tm_min != lastRenderedMinute) {
    lastRenderedMinute = tmNow.tm_min;
    if (pendingScope == RenderScope::Full) {
      // A full redraw is already pending (e.g. toast just dismissed); the
      // clock will be included in it, no need to downgrade.
    } else {
      pendingScope = RenderScope::ClockOnly;
    }
    requestUpdate();
  }
}

void LauncherActivity::handleConfirm() {
  const AppEntry& entry = APP_REGISTRY[selectorIndex];
  if (entry.enabled && entry.create) {
    entry.create();
  } else {
    toastVisible = true;
    toastShownAtMs = millis();
    pendingScope = RenderScope::Full;
    requestUpdate();
  }
}

void LauncherActivity::handleBack() {
  // Quick-Resume-equivalent: jump back into the last-open book, if any.
  if (!APP_STATE.openEpubPath.empty()) {
    activityManager.goToReader(APP_STATE.openEpubPath);
  }
}

Rect LauncherActivity::computeClockRect() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  return Rect{0, 0, pageWidth, pageHeight / 4};
}

Rect LauncherActivity::computeListRect() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto hintInsets = GUI.getButtonHintInsets(renderer);
  const Rect clockRect = computeClockRect();

  const int listTop = clockRect.y + clockRect.height;
  const int listBottom = pageHeight - hintInsets.bottom;
  const int listLeft = hintInsets.left;
  const int listWidth = pageWidth - hintInsets.left - hintInsets.right;
  return Rect{listLeft, listTop, listWidth, std::max(0, listBottom - listTop)};
}

void LauncherActivity::renderClockArea() const {
  const Rect clockRect = computeClockRect();
  const auto& metrics = UITheme::getInstance().getMetrics();

  char timeStr[8];
  char dateStr[32];
  if (ClockSync::isTimeSynced()) {
    struct tm tmNow{};
    const time_t localNow = time(nullptr) + ClockSync::TZ_OFFSET_MINUTES * 60;
    gmtime_r(&localNow, &tmNow);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d (%a)", &tmNow);
  } else {
    snprintf(timeStr, sizeof(timeStr), "--:--");
    snprintf(dateStr, sizeof(dateStr), "%s", I18N.get(StrId::STR_TIME_UNSYNCED));
  }

  const int timeLineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const int timeY = clockRect.y + metrics.topPadding + 8;
  renderer.drawCenteredText(UI_10_FONT_ID, timeY, timeStr);

  const int dateY = timeY + timeLineHeight + metrics.verticalSpacing / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, dateY, dateStr);

  // Battery indicator, top-right of the clock area.
  const int battX = clockRect.x + clockRect.width - metrics.batteryWidth - metrics.contentSidePadding;
  const int battY = clockRect.y + metrics.topPadding;
  GUI.drawBatteryRight(renderer, Rect{battX, battY, metrics.batteryWidth, metrics.batteryHeight}, true);
}

void LauncherActivity::renderListArea() const {
  const Rect listRect = computeListRect();

  constexpr int centerRowHeight = 60;
  constexpr int sideRowHeight = 34;
  constexpr int sideRowSpacing = 6;
  constexpr int visibleSideCount = 2;  // items shown above/below the centered selection
  constexpr int iconSize = 32;
  constexpr int iconMargin = 12;
  constexpr int indicatorHeight = 16;

  const int centerY = listRect.y + (listRect.height - indicatorHeight - centerRowHeight) / 2;

  auto drawRow = [&](int idx, int y, int rowHeight, bool selected) {
    const AppEntry& entry = APP_REGISTRY[idx];
    std::string label = I18N.get(entry.nameKey);
    // Built-in CJK glyphs are a fixed 20x20px bitmap regardless of which UI_xx
    // font ID is requested, so there is no size distinction available between
    // "large center" and "small side" rows -- UI_10 is used throughout since
    // it is the one UI font ID with confirmed working inverted-CJK rendering
    // elsewhere in this codebase (HomeActivity's drawButtonMenu). Emphasis for
    // the selected/center row comes from the larger fillRect + inversion only.
    const int fontId = UI_10_FONT_ID;

    if (selected) {
      renderer.fillRect(listRect.x, y, listRect.width, rowHeight);
    }

    const int iconX = listRect.x + iconMargin;
    const int iconY = y + (rowHeight - iconSize) / 2;
    if (entry.icon) {
      renderer.drawIcon(entry.icon, iconX, iconY, iconSize, iconSize);
    } else {
      renderer.drawRect(iconX, iconY, iconSize, iconSize, selected);
      const std::string initial = firstUtf8Char(label.c_str());
      const int initialWidth = renderer.getTextWidth(fontId, initial.c_str());
      const int initialHeight = renderer.getLineHeight(fontId);
      renderer.drawText(fontId, iconX + (iconSize - initialWidth) / 2, iconY + (iconSize - initialHeight) / 2,
                        initial.c_str(), !selected);
    }

    const int textX = iconX + iconSize + iconMargin;
    const int textHeight = renderer.getLineHeight(fontId);
    const int textY = y + (rowHeight - textHeight) / 2;
    renderer.drawText(fontId, textX, textY, label.c_str(), !selected);
  };

  drawRow(selectorIndex, centerY, centerRowHeight, true);

  int y = centerY;
  for (int i = 1; i <= visibleSideCount; ++i) {
    y -= (sideRowHeight + sideRowSpacing);
    if (y + sideRowHeight < listRect.y) break;
    drawRow(wrapIndex(selectorIndex - i, APP_REGISTRY_COUNT), y, sideRowHeight, false);
  }

  y = centerY + centerRowHeight + sideRowSpacing;
  for (int i = 1; i <= visibleSideCount; ++i) {
    if (y + sideRowHeight > listRect.y + listRect.height - indicatorHeight) break;
    drawRow(wrapIndex(selectorIndex + i, APP_REGISTRY_COUNT), y, sideRowHeight, false);
    y += sideRowHeight + sideRowSpacing;
  }

  // Position indicator: small dot per app, filled for the selected one, plus "n/total".
  constexpr int dotSize = 6;
  constexpr int dotSpacing = 10;
  const int dotsWidth = APP_REGISTRY_COUNT * dotSpacing;
  const int dotsX = listRect.x + (listRect.width - dotsWidth) / 2;
  const int dotsY = listRect.y + listRect.height - indicatorHeight;
  for (int i = 0; i < APP_REGISTRY_COUNT; ++i) {
    const int dotX = dotsX + i * dotSpacing;
    if (i == selectorIndex) {
      renderer.fillRect(dotX, dotsY, dotSize, dotSize);
    } else {
      renderer.drawRect(dotX, dotsY, dotSize, dotSize);
    }
  }

  char positionLabel[16];
  snprintf(positionLabel, sizeof(positionLabel), "%d/%d", selectorIndex + 1, APP_REGISTRY_COUNT);
  const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, positionLabel);
  renderer.drawText(UI_10_FONT_ID, listRect.x + listRect.width - labelWidth, dotsY, positionLabel);
}

void LauncherActivity::renderToast() const {
  const Rect popup = GUI.drawPopup(renderer, tr(STR_COMING_SOON_TOAST));
  (void)popup;
}

void LauncherActivity::render(RenderLock&&) {
  switch (pendingScope) {
    case RenderScope::Full: {
      renderer.clearScreen();
      renderClockArea();
      renderListArea();
      if (toastVisible) {
        renderToast();
      }
      if (renderer.isDarkMode()) {
        renderer.displayBufferDarkRedrive();
      } else {
        renderer.displayBuffer(HalDisplay::HALF_REFRESH);
      }
      break;
    }
    case RenderScope::ListOnly: {
      const Rect listRect = computeListRect();
      renderer.fillRect(listRect.x, listRect.y, listRect.width, listRect.height, false);
      renderListArea();
      renderer.setPartialUpdateRect(listRect.x, listRect.y, listRect.width, listRect.height);
      displayWithFixedRefreshCycle(renderer, listMovesUntilFullRefresh, LIST_REFRESH_CYCLE_N);
      break;
    }
    case RenderScope::ClockOnly: {
      const Rect clockRect = computeClockRect();
      renderer.fillRect(clockRect.x, clockRect.y, clockRect.width, clockRect.height, false);
      renderClockArea();
      renderer.setPartialUpdateRect(clockRect.x, clockRect.y, clockRect.width, clockRect.height);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
      break;
    }
  }
  pendingScope = RenderScope::Full;
}
