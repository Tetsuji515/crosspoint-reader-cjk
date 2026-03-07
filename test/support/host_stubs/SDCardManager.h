#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using oflag_t = uint8_t;
constexpr oflag_t O_RDONLY = 0x01;

namespace HostStorage {
inline std::unordered_map<std::string, std::vector<uint8_t>>& files() {
  static std::unordered_map<std::string, std::vector<uint8_t>> instance;
  return instance;
}

inline void clear() { files().clear(); }

inline void registerFile(const std::string& path, const std::vector<uint8_t>& data) { files()[path] = data; }
}  // namespace HostStorage

class FsFile {
 public:
  FsFile() = default;

  explicit operator bool() const { return static_cast<bool>(data_); }

  void close() {
    data_.reset();
    position_ = 0;
  }

  bool seek(uint32_t offset) {
    if (!data_ || offset > data_->size()) {
      return false;
    }
    position_ = offset;
    return true;
  }

  size_t read(void* buffer, size_t length) {
    if (!data_ || !buffer) {
      return 0;
    }

    const size_t remaining = data_->size() - position_;
    const size_t toRead = (length < remaining) ? length : remaining;
    if (toRead > 0) {
      std::memcpy(buffer, data_->data() + position_, toRead);
      position_ += toRead;
    }
    return toRead;
  }

 private:
  friend class SDCardManager;

  std::shared_ptr<std::vector<uint8_t>> data_;
  size_t position_ = 0;
};

class SDCardManager {
 public:
  static SDCardManager& getInstance() {
    static SDCardManager instance;
    return instance;
  }

  bool openFileForRead(const char*, const char* path, FsFile& file) {
    auto it = HostStorage::files().find(path);
    if (it == HostStorage::files().end()) {
      file.close();
      return false;
    }

    file.data_ = std::make_shared<std::vector<uint8_t>>(it->second);
    file.position_ = 0;
    return true;
  }
};
