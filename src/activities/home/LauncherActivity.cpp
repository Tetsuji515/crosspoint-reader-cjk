#include "LauncherActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cctype>
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
int wrapIndex(int idx, int count) {
  idx %= count;
  if (idx < 0) idx += count;
  return idx;
}

// The launcher UI is intentionally English + ASCII so every glyph comes from the
// scalable UI font at one consistent size (built-in CJK glyphs are a fixed 20x20
// bitmap and would clash with smaller ASCII text). These helpers add uniform
// letter-spacing ("tracking") for the near-future / HUD look. ASCII-only.
int trackedTextWidth(const GfxRenderer& r, int fontId, const char* s, int tracking,
                     EpdFontFamily::Style style) {
  int w = 0;
  for (const char* p = s; *p; ++p) {
    const char buf[2] = {*p, '\0'};
    w += r.getTextWidth(fontId, buf, style) + tracking;
  }
  return w > 0 ? w - tracking : 0;
}

void drawTrackedText(const GfxRenderer& r, int fontId, int x, int y, const char* s, int tracking, bool black,
                     EpdFontFamily::Style style) {
  int cx = x;
  for (const char* p = s; *p; ++p) {
    const char buf[2] = {*p, '\0'};
    r.drawText(fontId, cx, y, buf, black, style);
    cx += r.getTextWidth(fontId, buf, style) + tracking;
  }
}

constexpr int daysInMonth(int year, int month1to12) {
  constexpr int DIM[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  return (month1to12 == 2 && leap) ? 29 : DIM[month1to12 - 1];
}
}  // namespace

void LauncherActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  listMovesUntilFullRefresh = LIST_REFRESH_CYCLE_N;
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

  // Back is intentionally a no-op here: the launcher is the home/root screen,
  // so there is nowhere to go back to. (It used to reopen the last book, but
  // that surprised users returning from apps -- see docs/dev-notes/mem-investigation.md.)
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

Rect LauncherActivity::computeListRect() const {
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto hintInsets = GUI.getButtonHintInsets(renderer);

  const int listTop = HEADER_HEIGHT;
  const int listBottom = pageHeight - hintInsets.bottom;
  const int listLeft = hintInsets.left;
  const int listWidth = pageWidth - hintInsets.left - hintInsets.right;
  return Rect{listLeft, listTop, listWidth, std::max(0, listBottom - listTop)};
}

void LauncherActivity::renderHeader() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pad = metrics.contentSidePadding;
  const int W = renderer.getScreenWidth();
  const int fontId = LAUNCHER_FONT_ID;
  const int lineHeight = renderer.getLineHeight(fontId);

  // --- Top row: wordmark + battery -----------------------------------------
  const int titleY = metrics.topPadding + 6;
  constexpr int blockW = 6;
  renderer.fillRect(pad, titleY, blockW, lineHeight, true);
  drawTrackedText(renderer, fontId, pad + blockW + 12, titleY, "CROSSPOINT", 5, true, EpdFontFamily::BOLD);
  const int battX = W - pad - metrics.batteryWidth;
  GUI.drawBatteryRight(renderer, Rect{battX, titleY, metrics.batteryWidth, metrics.batteryHeight}, true);

  // --- Digital-watch bezel with a month calendar -----------------------------
  const int bezelX = pad;
  const int bezelY = titleY + lineHeight + 8;
  const int bezelW = W - 2 * pad;
  const int bezelH = HEADER_HEIGHT - bezelY - 6;
  renderer.drawRoundedRect(bezelX, bezelY, bezelW, bezelH, 2 /*lineWidth*/, 16, true);
  renderer.drawRoundedRect(bezelX + 5, bezelY + 5, bezelW - 10, bezelH - 10, 1 /*lineWidth*/, 11, true);

  // Resolve the date from the shared clock source (NTP-synced during any WiFi
  // connection). Unsynced -> dashed labels and an empty grid. The date changes
  // at most once a day, so there is no minute-boundary refresh machinery.
  const bool synced = ClockSync::isTimeSynced();
  struct tm tmNow{};
  char monthStr[8] = {'-', '-', '-', '\0'};
  char yearStr[8] = {'-', '-', '-', '-', '\0'};
  int firstWday = 0;  // column of day 1: back up today's weekday by (mday - 1) days
  int numDays = 0;
  int weekRows = 6;
  if (synced) {
    const time_t localNow = time(nullptr) + ClockSync::TZ_OFFSET_MINUTES * 60;
    gmtime_r(&localNow, &tmNow);
    strftime(monthStr, sizeof(monthStr), "%b", &tmNow);  // e.g. "Jul"
    for (char* c = monthStr; *c; ++c) *c = static_cast<char>(std::toupper(static_cast<unsigned char>(*c)));
    strftime(yearStr, sizeof(yearStr), "%Y", &tmNow);
    firstWday = ((tmNow.tm_wday - (tmNow.tm_mday - 1)) % 7 + 7) % 7;
    numDays = daysInMonth(tmNow.tm_year + 1900, tmNow.tm_mon + 1);
    weekRows = (firstWday + numDays + 6) / 7;  // 4-6 rows depending on the month
  }

  // Labels inside the bezel: month (top-left), year (top-right).
  constexpr int labelFont = NOTOSANS_14_FONT_ID;
  const int labelY = bezelY + 10;
  drawTrackedText(renderer, labelFont, bezelX + 22, labelY, monthStr, 3, true, EpdFontFamily::BOLD);
  const int yearW = trackedTextWidth(renderer, labelFont, yearStr, 2, EpdFontFamily::REGULAR);
  drawTrackedText(renderer, labelFont, bezelX + bezelW - 22 - yearW, labelY, yearStr, 2, true,
                  EpdFontFamily::REGULAR);

  // Calendar grid: weekday header + the month's week rows, Sunday-first
  // columns. Dividing the fixed grid height by the actual number of week rows
  // gives most months taller cells. If a 6-week month leaves rows shorter than
  // the 12pt line, fall back to the 8pt cut so digits never collide.
  constexpr int gridPad = 22;
  const int gridLeft = bezelX + gridPad;
  const int colW = (bezelW - 2 * gridPad) / 7;
  const int gridTop = labelY + renderer.getLineHeight(labelFont) + 4;
  const int gridBottom = bezelY + bezelH - 10;
  const int headerH = renderer.getLineHeight(SMALL_FONT_ID) + 4;
  const int rowH = (gridBottom - gridTop - headerH) / weekRows;
  int dayFont = NOTOSANS_12_FONT_ID;
  int dayLineHeight = renderer.getLineHeight(dayFont);
  if (rowH < dayLineHeight - 3) {
    dayFont = SMALL_FONT_ID;
    dayLineHeight = renderer.getLineHeight(dayFont);
  }

  static constexpr const char* WEEKDAY_LETTERS[7] = {"S", "M", "T", "W", "T", "F", "S"};
  for (int col = 0; col < 7; ++col) {
    const int w = renderer.getTextWidth(SMALL_FONT_ID, WEEKDAY_LETTERS[col], EpdFontFamily::REGULAR);
    renderer.drawText(SMALL_FONT_ID, gridLeft + col * colW + (colW - w) / 2, gridTop, WEEKDAY_LETTERS[col], true,
                      EpdFontFamily::REGULAR);
  }
  // Thin rule between the weekday header and the day cells.
  renderer.drawLine(gridLeft, gridTop + headerH - 2, gridLeft + 7 * colW, gridTop + headerH - 2, true);

  if (!synced) return;

  for (int day = 1; day <= numDays; ++day) {
    const int cell = firstWday + day - 1;
    const int cellX = gridLeft + (cell % 7) * colW;
    const int cellY = gridTop + headerH + (cell / 7) * rowH;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", day);
    const int w = renderer.getTextWidth(dayFont, buf, EpdFontFamily::REGULAR);
    const int textX = cellX + (colW - w) / 2;
    const int textY = cellY + (rowH - dayLineHeight) / 2;
    const bool isToday = day == tmNow.tm_mday;
    if (isToday) {
      // Inverted pill marks today.
      renderer.fillRoundedRect(cellX + 2, cellY + 1, colW - 4, rowH - 2, (rowH - 2) / 2, Color::Black);
    }
    renderer.drawText(dayFont, textX, textY, buf, !isToday, EpdFontFamily::REGULAR);
  }
}

namespace {
// Per-distance style for the carousel: rows nearer the centered selection are
// larger; rows further away shrink and indent to the right, tracing a gentle
// arc (like the reference photo). Index 0 == selected/centre. NotoSans built-in
// sizes are 18/16/14/12/8pt; the 8pt cut (SMALL_FONT_ID, regular only) is the
// closest available to "half of the selected 18pt", which keeps the selection
// clearly dominant.
struct RowStyle {
  int fontId;
  EpdFontFamily::Style style;
  int rowHeight;
  int arcIndent;  // extra left inset (px) -> the arc
};
RowStyle rowStyleFor(int distance) {
  switch (distance) {
    case 0:  return {NOTOSANS_18_FONT_ID, EpdFontFamily::BOLD, 68, 0};
    case 1:  return {NOTOSANS_12_FONT_ID, EpdFontFamily::REGULAR, 44, 18};
    case 2:  return {SMALL_FONT_ID, EpdFontFamily::REGULAR, 34, 46};
    default: return {SMALL_FONT_ID, EpdFontFamily::REGULAR, 30, 80};
  }
}
}  // namespace

void LauncherActivity::renderListArea() const {
  const Rect listRect = computeListRect();

  constexpr int visibleSideCount = 3;  // rows shown above/below the centered selection
  constexpr int rowGap = 8;
  constexpr int railWidth = 3;
  constexpr int railGap = 14;
  constexpr int rowPad = 14;
  constexpr int chevronColW = 20;

  const int listContentBottom = listRect.y + listRect.height;
  const int barX = listRect.x;
  const int barRight = listRect.x + listRect.width - railGap - railWidth;

  auto drawRow = [&](int idx, int distance, int top) {
    const AppEntry& entry = APP_REGISTRY[idx];
    const RowStyle s = rowStyleFor(distance < 0 ? -distance : distance);
    const bool selected = distance == 0;
    const bool black = !selected;

    const int rowLeft = barX + s.arcIndent;
    const int lineHeight = renderer.getLineHeight(s.fontId);
    const int textY = top + (s.rowHeight - lineHeight) / 2;

    if (selected) {
      renderer.fillRoundedRect(rowLeft, top, barRight - rowLeft, s.rowHeight, 12, Color::Black);
    }

    int x = rowLeft + rowPad;
    if (selected) {
      renderer.drawText(s.fontId, x, textY, ">", black, s.style);
    }
    x = rowLeft + rowPad + chevronColW;

    char indexBuf[4];
    snprintf(indexBuf, sizeof(indexBuf), "%02d", idx + 1);
    renderer.drawText(s.fontId, x, textY, indexBuf, black, s.style);
    const int indexW = renderer.getTextWidth(s.fontId, indexBuf, s.style);

    // Thin divider between index and label.
    const int divX = x + indexW + 8;
    const int half = lineHeight / 3;
    renderer.drawLine(divX, top + s.rowHeight / 2 - half, divX, top + s.rowHeight / 2 + half, black);

    drawTrackedText(renderer, s.fontId, divX + 12, textY, entry.label, 1, black, s.style);
  };

  // Centre row, then stack outward using each row's own (shrinking) height.
  // The stack is symmetric, so centering the centre row centers the whole
  // stack within the list area (the lower 2/3 of the screen).
  const RowStyle centre = rowStyleFor(0);
  int heightAbove = 0;
  for (int d = 1; d <= visibleSideCount; ++d) {
    heightAbove += rowStyleFor(d).rowHeight + rowGap;
  }
  const int centreTop = listRect.y + (listContentBottom - listRect.y - centre.rowHeight) / 2;
  drawRow(selectorIndex, 0, centreTop);

  int prevTop = centreTop;
  for (int d = 1; d <= visibleSideCount; ++d) {
    const int h = rowStyleFor(d).rowHeight;
    const int top = prevTop - rowGap - h;
    if (top + h < listRect.y) break;
    drawRow(wrapIndex(selectorIndex - d, APP_REGISTRY_COUNT), -d, top);
    prevTop = top;
  }

  int prevBottom = centreTop + centre.rowHeight;
  for (int d = 1; d <= visibleSideCount; ++d) {
    const RowStyle s = rowStyleFor(d);
    const int top = prevBottom + rowGap;
    if (top + s.rowHeight > listContentBottom) break;
    drawRow(wrapIndex(selectorIndex + d, APP_REGISTRY_COUNT), d, top);
    prevBottom = top + s.rowHeight;
  }

  // Right-edge scroll rail with a position knob, spanning the visible stack.
  const int railX = listRect.x + listRect.width - railWidth;
  const int railTop = std::max(listRect.y, centreTop - heightAbove);
  const int railBottom = std::min(listContentBottom, centreTop + centre.rowHeight + heightAbove);
  renderer.drawLine(railX, railTop, railX, railBottom, true);
  const int railTravel = railBottom - railTop;
  const int knobH = std::max(10, railTravel / APP_REGISTRY_COUNT);
  const int knobY = railTop + (railTravel - knobH) * selectorIndex / std::max(1, APP_REGISTRY_COUNT - 1);
  renderer.fillRect(railX - 2, knobY, railWidth + 2, knobH, true);
}

void LauncherActivity::renderToast() const {
  const Rect popup = GUI.drawPopup(renderer, tr(STR_COMING_SOON_TOAST));
  (void)popup;
}

void LauncherActivity::render(RenderLock&&) {
  switch (pendingScope) {
    case RenderScope::Full: {
      renderer.clearScreen();
      renderHeader();
      renderListArea();
      // Physical front-button roles along the bottom edge. Back is a no-op on
      // the home screen, so its slot stays blank. Labels are ASCII on purpose
      // (see the tracked-text helpers above); mapLabels() reorders them to the
      // user's configured hardware layout.
      const auto labels = mappedInput.mapLabels("", "OPEN", "UP", "DOWN");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
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
  }
  pendingScope = RenderScope::Full;
}
