#include "MappedInputManager.h"

#include "CrossPointSettings.h"
#include "input/BluetoothPageTurnState.h"

// Button mapping is split into three orthogonal concerns:
//
//   1. **Hardware spec**  — which GPIO index sits at which physical position
//      on the X4 device. Source of truth: the `kFront*` / `kSide*` constants
//      below. This is the only place that touches HalGPIO::BTN_* values.
//
//   2. **User remap**     — the user can re-assign the four front-bezel
//      "roles" (Back / Confirm / Left / Right) to arbitrary hardware indices,
//      and choose a side-strip prev/next polarity (PREV_NEXT / NEXT_PREV).
//      Lives in `CrossPointSettings::frontButton*` and `sideButtonLayout`.
//
//   3. **Orientation**    — when the device is rotated/flipped, the same
//      physical button now sits at a different *visual* position. Each
//      logical button has its own rule for how to react to orientation:
//
//          | Logical button         | Inverted | LandscapeCW | LandscapeCCW |
//          |------------------------|----------|-------------|--------------|
//          | Back / Confirm         | mirror   |    —        |     —        |
//          | Left / Right           | mirror   |    —        |  swap roles  |
//          | Up / Down              | swap     |    —        |     —        |
//          | PageBack / PageForward | swap     |   swap      |   swap       |
//          | Power                  |    —     |    —        |     —        |
//
//      "mirror" = front-bezel hw index 0..3 is reflected (3 - i).
//      "swap"   = the user's prev/next polarity is inverted at the call site.
//      "swap roles" = the role looked up in SETTINGS is the opposite arrow
//      (Left looks up Right, Right looks up Left), without an index mirror.
//
// Each rule is documented next to the corresponding case below so the
// behaviour can be audited without reading the whole file.

namespace {
using ButtonIndex = uint8_t;
using Orientation = MappedInputManager::Orientation;

// === Hardware spec (X4) ===
//
// Front bezel: four buttons along the bottom edge of the device when held
// in portrait, ordered left-to-right with hardware indices 0..3. The
// HalGPIO names happen to follow this physical order.
//
// Side strip: two buttons on the right edge of the device when held in
// portrait, ordered top-to-bottom. ADC channel naming != physical position:
//   BTN_DOWN(5) is the physical UPPER button.
//   BTN_UP(4)   is the physical LOWER button.
constexpr ButtonIndex kSideUpper = HalGPIO::BTN_DOWN;
constexpr ButtonIndex kSideLower = HalGPIO::BTN_UP;

// === Side polarity (user-configurable) ===
//
// In PORTRAIT orientation, which physical side button is "previous" and
// which is "next". Indexed by CrossPointSettings::SIDE_BUTTON_LAYOUT.
struct SidePolarity {
  ButtonIndex prev;
  ButtonIndex next;
};
constexpr SidePolarity kSidePolarities[] = {
    {kSideUpper, kSideLower},  // PREV_NEXT: upper = prev, lower = next
    {kSideLower, kSideUpper},  // NEXT_PREV: reversed
};

// === Helpers ===
//
// Mirror a front-bezel hardware index across the centre (0<->3, 1<->2).
// Used when the device is held inverted: the user's role-assigned index
// is in portrait coordinates, so to address the same visual position we
// have to reflect it.
constexpr ButtonIndex mirrorFront(ButtonIndex idx) { return 3 - idx; }

// `isInverted`           — device flipped 180° relative to portrait.
//                          Triggers index/polarity mirroring for buttons
//                          whose visual position depends on orientation.
// `isLandscapeRotated`   — device rotated 90° (either direction). Side-
//                          strip paging buttons swap polarity in either
//                          rotation; front Left/Right swap only on CCW
//                          (the bezel ends up on the right edge with
//                          reversed top-to-bottom order).
constexpr bool isInverted(Orientation o) { return o == Orientation::PortraitInverted; }
constexpr bool isLandscapeRotated(Orientation o) {
  return o == Orientation::LandscapeClockwise || o == Orientation::LandscapeCounterClockwise;
}
constexpr bool isLandscapeCcw(Orientation o) { return o == Orientation::LandscapeCounterClockwise; }

// Resolve a user-assigned front-bezel role to its active hardware index.
// Inverted reflects the index; landscape rotations don't (the bezel stays
// addressable by the same hardware index, only its visual position moves).
constexpr ButtonIndex frontHwForRole(ButtonIndex roleIndex, Orientation o) {
  return isInverted(o) ? mirrorFront(roleIndex) : roleIndex;
}

}  // namespace

bool MappedInputManager::mapButton(const Button button, bool (HalGPIO::*fn)(uint8_t) const) const {
  const Orientation o = effectiveOrientation;
  const auto layout = static_cast<CrossPointSettings::SIDE_BUTTON_LAYOUT>(SETTINGS.sideButtonLayout);
  const SidePolarity& polarity = kSidePolarities[layout];

  switch (button) {
    // === Front-bezel action buttons =====================================
    // The user assigns each role (Back / Confirm) to a specific hardware
    // index via SETTINGS. Inverted mirrors the index; landscape rotations
    // do not — the user finds these buttons by their on-screen label,
    // which the renderer redraws at the correct edge for each orientation.
    case Button::Back:
      return (gpio.*fn)(frontHwForRole(SETTINGS.frontButtonBack, o));
    case Button::Confirm:
      return (gpio.*fn)(frontHwForRole(SETTINGS.frontButtonConfirm, o));

    // === Front-bezel directional buttons ================================
    // In LandscapeCCW the bezel rotates onto the device's right edge with
    // reversed top-to-bottom order, so the user's "Left" arrow addresses
    // the role that was originally on the right (and vice versa). The
    // index itself is NOT mirrored: in CCW rotation the role-assigned hw
    // index still hits the right physical button. In Inverted, both the
    // role-swap is unnecessary AND the index needs reflection.
    case Button::Left:
      if (isInverted(o)) return (gpio.*fn)(mirrorFront(SETTINGS.frontButtonLeft));
      if (isLandscapeCcw(o)) return (gpio.*fn)(SETTINGS.frontButtonRight);
      return (gpio.*fn)(SETTINGS.frontButtonLeft);
    case Button::Right:
      if (isInverted(o)) return (gpio.*fn)(mirrorFront(SETTINGS.frontButtonRight));
      if (isLandscapeCcw(o)) return (gpio.*fn)(SETTINGS.frontButtonLeft);
      return (gpio.*fn)(SETTINGS.frontButtonRight);

    // === Side-strip vertical (Up / Down) ================================
    // Used by ButtonNavigator as side-key alternatives to Left/Right for
    // chapter / menu navigation. Mapped by HARDWARE NAME (BTN_UP /
    // BTN_DOWN) rather than physical position, swapped only on the
    // device-flipped (Inverted) orientation. Landscape rotations don't
    // remap because chapter list logic treats them as direction-agnostic
    // alternates that move with the strip.
    case Button::Up:
      return (gpio.*fn)(isInverted(o) ? HalGPIO::BTN_DOWN : HalGPIO::BTN_UP);
    case Button::Down:
      return (gpio.*fn)(isInverted(o) ? HalGPIO::BTN_UP : HalGPIO::BTN_DOWN);

    // === Reader paging (PageBack / PageForward) =========================
    // Use the user's prev/next polarity, with the polarity inverted in
    // every non-portrait orientation so the side button visually closer
    // to the start-of-line stays mapped to "previous":
    //   - Inverted             — device flipped 180°, top↔bottom swapped.
    //   - LandscapeClockwise   — empirically validated by 78154ba.
    //   - LandscapeCCW         — empirically validated by fc63d2e.
    // Users who want a different mapping should toggle SIDE_BUTTON_LAYOUT
    // (PREV_NEXT ↔ NEXT_PREV) in settings.
    case Button::PageBack:
    case Button::PageBack:
      return (gpio.*fn)(o == Orientation::Portrait ? polarity.prev : polarity.next);
    case Button::PageForward:
      return (gpio.*fn)(o == Orientation::Portrait ? polarity.next : polarity.prev);

    // === Power ==========================================================
    case Button::Power:
      return (gpio.*fn)(HalGPIO::BTN_POWER);
  }

  return false;  // unreachable; switch covers all enum values.
}

bool MappedInputManager::checkButton(const Button button, const GpioFn gpioFn, const BtFn btPageBackFn,
                                     const BtFn btPageForwardFn) const {
  const bool physical = mapButton(button, gpioFn);
  if (!bluetoothPageTurnState) {
    return physical;
  }
  switch (button) {
    case Button::PageBack:
      return physical || (bluetoothPageTurnState->*btPageBackFn)();
    case Button::PageForward:
      return physical || (bluetoothPageTurnState->*btPageForwardFn)();
    default:
      return physical;
  }
}

bool MappedInputManager::wasPressed(const Button button) const {
  return checkButton(button, &HalGPIO::wasPressed, &BluetoothPageTurnState::wasPageBackPressed,
                     &BluetoothPageTurnState::wasPageForwardPressed);
}

bool MappedInputManager::wasReleased(const Button button) const {
  return checkButton(button, &HalGPIO::wasReleased, &BluetoothPageTurnState::wasPageBackReleased,
                     &BluetoothPageTurnState::wasPageForwardReleased);
}

bool MappedInputManager::isPressed(const Button button) const {
  return checkButton(button, &HalGPIO::isPressed, &BluetoothPageTurnState::isPageBackPressed,
                     &BluetoothPageTurnState::isPageForwardPressed);
}

bool MappedInputManager::wasAnyPressed() const {
  return gpio.wasAnyPressed() || (bluetoothPageTurnState && bluetoothPageTurnState->wasAnyPressed());
}

bool MappedInputManager::wasAnyReleased() const {
  return gpio.wasAnyReleased() || (bluetoothPageTurnState && bluetoothPageTurnState->wasAnyReleased());
}

unsigned long MappedInputManager::getHeldTime() const { return gpio.getHeldTime(); }

MappedInputManager::Labels MappedInputManager::mapLabels(const char* back, const char* confirm, const char* previous,
                                                         const char* next) const {
  // Build the label order based on the configured hardware mapping.
  // LandscapeCCW: front buttons rotate to right side (vertical). Physical
  // top-to-bottom becomes GPIO 3,2,1,0. drawButtonHints reverses labels
  // (0<->3, 1<->2) so that visual top = labels[3]. To make physical top = previous
  // (user expectation: up = previous page), we swap previous<->next in the label
  // assignment so that after drawButtonHints' reversal the labels match.
  const bool swapPrevNext = effectiveOrientation == Orientation::LandscapeCounterClockwise;
  const char* prev = swapPrevNext ? next : previous;
  const char* nxt = swapPrevNext ? previous : next;

  auto labelForHardware = [&](uint8_t hw) -> const char* {
    // Compare against configured logical roles and return the matching label.
    if (hw == SETTINGS.frontButtonBack) {
      return back;
    }
    if (hw == SETTINGS.frontButtonConfirm) {
      return confirm;
    }
    if (hw == SETTINGS.frontButtonLeft) {
      return prev;
    }
    if (hw == SETTINGS.frontButtonRight) {
      return nxt;
    }
    return "";
  };

  return {labelForHardware(HalGPIO::BTN_BACK), labelForHardware(HalGPIO::BTN_CONFIRM),
          labelForHardware(HalGPIO::BTN_LEFT), labelForHardware(HalGPIO::BTN_RIGHT)};
}

int MappedInputManager::getPressedFrontButton() const {
  // Scan the raw front buttons in hardware order.
  // This bypasses remapping so the remap activity can capture physical presses.
  if (gpio.wasPressed(HalGPIO::BTN_BACK)) {
    return HalGPIO::BTN_BACK;
  }
  if (gpio.wasPressed(HalGPIO::BTN_CONFIRM)) {
    return HalGPIO::BTN_CONFIRM;
  }
  if (gpio.wasPressed(HalGPIO::BTN_LEFT)) {
    return HalGPIO::BTN_LEFT;
  }
  if (gpio.wasPressed(HalGPIO::BTN_RIGHT)) {
    return HalGPIO::BTN_RIGHT;
  }
  return -1;
}
