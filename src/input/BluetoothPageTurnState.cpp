#include "BluetoothPageTurnState.h"

void BluetoothPageTurnState::clearFrameEvents() {
  pageBackPressedEvent = false;
  pageBackReleasedEvent = false;
  pageForwardPressedEvent = false;
  pageForwardReleasedEvent = false;
}

void BluetoothPageTurnState::reportKeyDown(const Key key) {
  switch (key) {
    case Key::PageBack:
      if (!pageBackPressed) {
        pageBackPressed = true;
        pageBackPressedEvent = true;
      }
      break;
    case Key::PageForward:
      if (!pageForwardPressed) {
        pageForwardPressed = true;
        pageForwardPressedEvent = true;
      }
      break;
  }
}

void BluetoothPageTurnState::reportKeyUp(const Key key) {
  switch (key) {
    case Key::PageBack:
      if (pageBackPressed) {
        pageBackPressed = false;
        pageBackReleasedEvent = true;
      }
      break;
    case Key::PageForward:
      if (pageForwardPressed) {
        pageForwardPressed = false;
        pageForwardReleasedEvent = true;
      }
      break;
  }
}
