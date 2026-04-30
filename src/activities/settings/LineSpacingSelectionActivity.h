#pragma once

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/ActivityResult.h"
#include "util/ButtonNavigator.h"

class LineSpacingSelectionActivity final : public Activity {
 public:
  explicit LineSpacingSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int initialValue)
      : Activity("LineSpacingSelection", renderer, mappedInput), value(initialValue) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int value = 100;
  ButtonNavigator buttonNavigator;

  void adjustValue(int delta);
};