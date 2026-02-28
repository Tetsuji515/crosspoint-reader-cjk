#pragma once

#include <HalGPIO.h>

class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };
  enum class Orientation { Portrait, PortraitInverted, LandscapeClockwise, LandscapeCounterClockwise };

  struct Labels {
    const char* btn1;
    const char* btn2;
    const char* btn3;
    const char* btn4;
  };

  explicit MappedInputManager(HalGPIO& gpio) : gpio(gpio) {}

  void update() const { gpio.update(); }
  bool wasPressed(Button button) const;
  bool wasReleased(Button button) const;
  bool isPressed(Button button) const;
  bool wasAnyPressed() const;
  bool wasAnyReleased() const;
  unsigned long getHeldTime() const;
  Labels mapLabels(const char* back, const char* confirm, const char* previous, const char* next) const;
  // Returns the raw front button index that was pressed this frame (or -1 if none).
  int getPressedFrontButton() const;

  // Set effective orientation to remap directional buttons for rotated display
  void setEffectiveOrientation(Orientation o) { effectiveOrientation = o; }

 private:
  HalGPIO& gpio;
  Orientation effectiveOrientation = Orientation::Portrait;

  bool mapButton(Button button, bool (HalGPIO::*fn)(uint8_t) const) const;
  // Remap directional button based on current screen orientation
  Button remapForOrientation(Button button) const;
};
