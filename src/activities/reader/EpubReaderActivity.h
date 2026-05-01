#pragma once
#include <Epub.h>
#include <Epub/Section.h>

#include "EpubReaderMenuActivity.h"
#include "activities/Activity.h"

class EpubReaderActivity final : public Activity {
  std::shared_ptr<Epub> epub;
  std::unique_ptr<Section> section = nullptr;
  int currentSpineIndex = 0;
  int nextPageNumber = 0;
  int pagesUntilFullRefresh = 0;
  int cachedSpineIndex = 0;
  int cachedChapterTotalPageCount = 0;
  // Signals that the next render should reposition within the newly loaded section
  // based on a cross-book percentage jump.
  bool pendingPercentJump = false;
  // Normalized 0.0-1.0 progress within the target spine item, computed from book percentage.
  float pendingSpineProgress = 0.0f;
  bool pendingGoHome = false;
  bool pendingScreenshot = false;
  bool skipNextButtonCheck = false;  // Skip button processing for one frame after subactivity exit

  void renderContents(std::unique_ptr<Page> page, int orientedMarginTop, int orientedMarginRight,
                      int orientedMarginBottom, int orientedMarginLeft);
  void renderStatusBar() const;
  void saveProgress(int spineIndex, int currentPage, int pageCount);
  // Loads the current spine item into `section`, building the cache if necessary,
  // and applies any pending percent/cached-progress jumps. Returns false when an
  // error path was already drawn (caller must return without rendering further).
  bool loadOrBuildSection(int orientedMarginTop, int orientedMarginRight, int orientedMarginBottom,
                          int orientedMarginLeft);
  // Jump to a percentage of the book (0-100), mapping it to spine and page.
  void jumpToPercent(int percent);
  void invalidateSectionPreservingPosition();
  void applyOrientation(uint8_t orientation);
  void showReaderMenu();
  void handleMenuResult(const ActivityResult& result);

  // Per-action menu handlers. Each handles one EpubReaderMenuActivity::MenuAction
  // case so handleMenuResult() stays a thin dispatcher.
  void onSelectChapter();
  void onGoToPercent();
  void onReaderSettings();
  void onToggleFirstLineIndent();
  void onToggleInvertImages();
  void onChangeFontFamily();
  void onChangeLineSpacing();
  void onStatusBarSettings();
  void onDisplayQr();
  void onGoHome();
  void onDeleteCache();
  void onScreenshot();
  void onSync();

 public:
  explicit EpubReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::unique_ptr<Epub> epub)
      : Activity("EpubReader", renderer, mappedInput), epub(std::move(epub)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;
  bool supportsLandscape() const override { return true; }
  bool isReaderActivity() const override { return true; }
};