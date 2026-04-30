#include "BluetoothPageTurnReportParser.h"

namespace {
constexpr uint8_t KEYBOARD_LEFT_ARROW = 0x50;
constexpr uint8_t KEYBOARD_RIGHT_ARROW = 0x4F;
constexpr uint8_t KEYBOARD_UP_ARROW = 0x52;
constexpr uint8_t KEYBOARD_DOWN_ARROW = 0x51;
constexpr uint8_t KEYBOARD_PAGE_UP = 0x4B;
constexpr uint8_t KEYBOARD_PAGE_DOWN = 0x4E;
constexpr uint8_t KEYBOARD_VOLUME_UP = 0x80;
constexpr uint8_t KEYBOARD_VOLUME_DOWN = 0x81;

constexpr uint16_t CONSUMER_VOLUME_UP = 0x00E9;
constexpr uint16_t CONSUMER_VOLUME_DOWN = 0x00EA;
}  // namespace

uint8_t BluetoothPageTurnReportParser::parseKeyboardReport(const uint8_t* data, const size_t length) {
  if (!data || length < 8) {
    return 0;
  }

  uint8_t mask = 0;
  for (size_t i = 2; i < 8; ++i) {
    switch (data[i]) {
      case KEYBOARD_LEFT_ARROW:
      case KEYBOARD_UP_ARROW:
      case KEYBOARD_PAGE_UP:
      case KEYBOARD_VOLUME_UP:
        mask |= PAGE_BACK_MASK;
        break;
      case KEYBOARD_RIGHT_ARROW:
      case KEYBOARD_DOWN_ARROW:
      case KEYBOARD_PAGE_DOWN:
      case KEYBOARD_VOLUME_DOWN:
        mask |= PAGE_FORWARD_MASK;
        break;
      default:
        break;
    }
  }

  return mask;
}

uint8_t BluetoothPageTurnReportParser::parseConsumerReport(const uint8_t* data, const size_t length) {
  if (!data || length == 0) {
    return 0;
  }

  const uint16_t usage = (length >= 2) ? static_cast<uint16_t>(data[0] | (data[1] << 8)) : data[0];
  switch (usage) {
    case CONSUMER_VOLUME_UP:
      return PAGE_BACK_MASK;
    case CONSUMER_VOLUME_DOWN:
      return PAGE_FORWARD_MASK;
    default:
      return 0;
  }
}
