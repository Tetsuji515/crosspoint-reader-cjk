#pragma once

#include <cstddef>
#include <cstdint>

class BluetoothPageTurnReportParser {
 public:
  static constexpr uint8_t PAGE_BACK_MASK = 0x01;
  static constexpr uint8_t PAGE_FORWARD_MASK = 0x02;

  static uint8_t parseKeyboardReport(const uint8_t* data, size_t length);
  static uint8_t parseConsumerReport(const uint8_t* data, size_t length);
};
