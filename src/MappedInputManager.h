#pragma once

#include <HalGPIO.h>

class BluetoothPageTurnState;

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

  explicit MappedInputManager(HalGPIO& gpio, const BluetoothPageTurnState* bluetoothPageTurnState = nullptr)
      : gpio(gpio), bluetoothPageTurnState(bluetoothPageTurnState) {}

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

  // Set the effective screen orientation (called by OrientationHelper when
  // switching activities). Button mapping uses this instead of the raw
  // SETTINGS.orientation so that UI pages in Portrait mode are not affected
  // by a landscape setting.
  void setEffectiveOrientation(Orientation o) { effectiveOrientation = o; }

 private:
  using GpioFn = bool (HalGPIO::*)(uint8_t) const;
  using BtFn = bool (BluetoothPageTurnState::*)() const;

  HalGPIO& gpio;
  const BluetoothPageTurnState* bluetoothPageTurnState = nullptr;
  Orientation effectiveOrientation = Orientation::Portrait;

  bool mapButton(Button button, GpioFn fn) const;
  // Returns the physical state for `button` and ORs in the matching
  // Bluetooth page-turn state when applicable.
  bool checkButton(Button button, GpioFn gpioFn, BtFn btPageBackFn, BtFn btPageForwardFn) const;
};
