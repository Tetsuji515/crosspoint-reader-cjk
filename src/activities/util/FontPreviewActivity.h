#pragma once

#include <functional>
#include <string>

#include "../Activity.h"
#include "MappedInputManager.h"

class FontPreviewActivity final : public Activity {
 public:
  FontPreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath,
                      std::function<void()> onGoBack);

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  std::string filePath;
  std::function<void()> onGoBack;
};
