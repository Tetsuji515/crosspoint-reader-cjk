#pragma once

class BluetoothPageTurnState {
 public:
  enum class Key { PageBack, PageForward };

  void clearFrameEvents();
  void reportKeyDown(Key key);
  void reportKeyUp(Key key);

  bool isPageBackPressed() const { return pageBackPressed; }
  bool isPageForwardPressed() const { return pageForwardPressed; }

  bool wasPageBackPressed() const { return pageBackPressedEvent; }
  bool wasPageBackReleased() const { return pageBackReleasedEvent; }
  bool wasPageForwardPressed() const { return pageForwardPressedEvent; }
  bool wasPageForwardReleased() const { return pageForwardReleasedEvent; }

  bool wasAnyPressed() const { return pageBackPressedEvent || pageForwardPressedEvent; }
  bool wasAnyReleased() const { return pageBackReleasedEvent || pageForwardReleasedEvent; }

 private:
  bool pageBackPressed = false;
  bool pageForwardPressed = false;
  bool pageBackPressedEvent = false;
  bool pageBackReleasedEvent = false;
  bool pageForwardPressedEvent = false;
  bool pageForwardReleasedEvent = false;
};
