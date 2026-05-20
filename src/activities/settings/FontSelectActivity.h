#pragma once

#include "activities/Activity.h"
#include "activities/ActivityResult.h"

/**
 * Font selection page
 * Shows available external fonts and allows user to select
 * Uses synchronous rendering (no background task) to avoid FreeRTOS conflicts
 */
class FontSelectActivity final : public Activity {
 public:
  enum class SelectMode { Reader, UI };
  explicit FontSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, SelectMode mode)
      : Activity("FontSelect", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  SelectMode mode;
  int selectedIndex = 0;  // Index in the current list
  int totalItems = 1;     // At least one built-in option

  void openFontPreview();
};
