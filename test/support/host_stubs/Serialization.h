#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace serialization {

template <typename File, typename T>
inline bool writePod(File&, const T&) {
  return true;
}

template <typename File, typename T>
inline bool readPod(File&, T&) {
  return false;
}

template <typename File>
inline bool writeString(File&, const std::string&) {
  return true;
}

template <typename File>
inline bool readString(File&, std::string&) {
  return false;
}

}  // namespace serialization
