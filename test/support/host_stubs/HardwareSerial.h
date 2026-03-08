#pragma once

class HostSerial {
 public:
  template <typename... Args>
  void printf(const char*, Args...) {}
};

inline HostSerial Serial;
