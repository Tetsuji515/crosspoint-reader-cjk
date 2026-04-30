#include "FontPreviewActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include "components/UITheme.h"
#include "fontIds.h"

namespace {

std::string getDisplayName(const std::string& path) {
  const size_t slashPos = path.find_last_of('/');
  if (slashPos == std::string::npos) {
    return path;
  }
  return path.substr(slashPos + 1);
}

// All characters below are in CjkUiFont20 (hiragana, katakana, ASCII, built-in CJK subset).
// This ensures rendering uses ONLY the fast built-in PROGMEM font — no external font SD reads.
// 文字体大小 — all confirmed in CjkUiFont20 (0x6587, 0x5B57, 0x4F53, 0x5927, 0x5C0F)
const char* SAMPLE_LINE_CJK = "\xe6\x96\x87\xe5\xad\x97\xe4\xbd\x93\xe5\xa4\xa7\xe5\xb0\x8f";
const char* SAMPLE_LINE_KANA =
    "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a"    // あいうえお
    " \xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa";  // アイウエオ
const char* SAMPLE_LINE_ASCII = "ABCabc 0123456789 !@#";

}  // namespace

FontPreviewActivity::FontPreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path,
                                         std::function<void()> onGoBack)
    : Activity("FontPreview", renderer, mappedInput), filePath(std::move(path)), onGoBack(std::move(onGoBack)) {}

void FontPreviewActivity::onEnter() {
  LOG_DBG("FNTPREV", "onEnter start");
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const std::string displayName = getDisplayName(filePath);

  renderer.clearScreen();
  LOG_DBG("FNTPREV", "drawHeader...");
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, displayName.c_str(),
                 "Font preview");

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 20;
  const int lineSpacing = renderer.getLineHeight(UI_12_FONT_ID) + 12;

  LOG_DBG("FNTPREV", "drawCenteredText CJK...");
  renderer.drawCenteredText(UI_12_FONT_ID, contentTop, SAMPLE_LINE_CJK, true, EpdFontFamily::BOLD);
  LOG_DBG("FNTPREV", "drawCenteredText Kana...");
  renderer.drawCenteredText(UI_12_FONT_ID, contentTop + lineSpacing, SAMPLE_LINE_KANA);
  LOG_DBG("FNTPREV", "drawCenteredText ASCII...");
  renderer.drawCenteredText(UI_10_FONT_ID, contentTop + lineSpacing * 2, SAMPLE_LINE_ASCII);

  LOG_DBG("FNTPREV", "drawButtonHints...");
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  LOG_DBG("FNTPREV", "displayBuffer FULL_REFRESH...");
  renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  LOG_DBG("FNTPREV", "onEnter done");
}

void FontPreviewActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void FontPreviewActivity::loop() {
  Activity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (onGoBack) onGoBack();
    return;
  }
}
