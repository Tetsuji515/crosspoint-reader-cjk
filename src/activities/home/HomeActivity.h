#pragma once
#include <functional>
#include <vector>

#include "../Activity.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  bool firstRenderDone = false;
  bool hasOpdsUrl = false;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  bool coverBufferDarkMode = false;
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  std::vector<RecentBook> recentBooks;

  // Ignore Back until any press-release that started before this activity was
  // entered has fully cleared. Some activities (e.g. the reader) exit on Back
  // *press*, and the matching release would otherwise bleed into the Back
  // handler here and immediately bounce to the launcher. Mirrors the guard in
  // LauncherActivity::loop().
  bool skipBackUntilClear = true;

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void loadRecentBooks(int maxBooks);
  void loadRecentCovers(int coverHeight);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Home", renderer, mappedInput) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
};
