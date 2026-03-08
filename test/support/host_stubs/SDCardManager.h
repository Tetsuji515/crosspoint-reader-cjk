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

inline size_t& seekCount() {
  static size_t instance = 0;
  return instance;
}

inline size_t& readCount() {
  static size_t instance = 0;
  return instance;
}

inline void clear() {
  files().clear();
  seekCount() = 0;
  readCount() = 0;
}

inline void registerFile(const std::string& path, const std::vector<uint8_t>& data) { files()[path] = data; }
inline void resetIoCounters() {
  seekCount() = 0;
  readCount() = 0;
}
inline size_t getSeekCount() { return seekCount(); }
inline size_t getReadCount() { return readCount(); }
}  // namespace HostStorage

class FsFile {
 public:
  FsFile() = default;

  explicit operator bool() const { return isDir_ || static_cast<bool>(data_); }

  void close() {
    data_.reset();
    isDir_ = false;
    directoryEntries_.clear();
    directoryIndex_ = 0;
    path_.clear();
    name_.clear();
    position_ = 0;
  }

  bool seek(uint32_t offset) {
    if (!data_ || offset > data_->size()) {
      return false;
    }
    HostStorage::seekCount() += 1;
    position_ = offset;
    return true;
  }

  size_t read(void* buffer, size_t length) {
    if (!data_ || !buffer) {
      return 0;
    }

    HostStorage::readCount() += 1;
    const size_t remaining = data_->size() - position_;
    const size_t toRead = (length < remaining) ? length : remaining;
    if (toRead > 0) {
      std::memcpy(buffer, data_->data() + position_, toRead);
      position_ += toRead;
    }
    return toRead;
  }

  bool isDir() const { return isDir_; }

  bool openNext(FsFile* dir, oflag_t) {
    if (!dir || !dir->isDir_ || dir->directoryIndex_ >= dir->directoryEntries_.size()) {
      close();
      return false;
    }

    const std::string childPath = dir->directoryEntries_[dir->directoryIndex_++];
    const auto it = HostStorage::files().find(childPath);
    if (it == HostStorage::files().end()) {
      close();
      return false;
    }

    data_ = std::make_shared<std::vector<uint8_t>>(it->second);
    isDir_ = false;
    directoryEntries_.clear();
    directoryIndex_ = 0;
    path_ = childPath;
    position_ = 0;

    const size_t slash = childPath.find_last_of('/');
    name_ = (slash == std::string::npos) ? childPath : childPath.substr(slash + 1);
    return true;
  }

  bool getName(char* buffer, size_t length) const {
    if (!buffer || length == 0) {
      return false;
    }
    if (name_.size() + 1 > length) {
      return false;
    }
    std::memcpy(buffer, name_.c_str(), name_.size() + 1);
    return true;
  }

 private:
  friend class SDCardManager;

  std::shared_ptr<std::vector<uint8_t>> data_;
  bool isDir_ = false;
  std::vector<std::string> directoryEntries_;
  size_t directoryIndex_ = 0;
  std::string path_;
  std::string name_;
  size_t position_ = 0;
};

class SDCardManager {
 public:
  static SDCardManager& getInstance() {
    static SDCardManager instance;
    return instance;
  }

  FsFile open(const char* path, oflag_t) {
    FsFile file;
    if (!path) {
      return file;
    }

    std::string prefix = path;
    if (!prefix.empty() && prefix.back() != '/') {
      prefix.push_back('/');
    }

    std::vector<std::string> entries;
    for (const auto& [filePath, _] : HostStorage::files()) {
      if (filePath.rfind(prefix, 0) != 0) {
        continue;
      }
      const std::string rest = filePath.substr(prefix.size());
      if (rest.empty() || rest.find('/') != std::string::npos) {
        continue;
      }
      entries.push_back(filePath);
    }

    if (entries.empty()) {
      return file;
    }

    file.isDir_ = true;
    file.directoryEntries_ = std::move(entries);
    file.directoryIndex_ = 0;
    file.path_ = path;
    const size_t slash = file.path_.find_last_of('/');
    file.name_ = (slash == std::string::npos) ? file.path_ : file.path_.substr(slash + 1);
    return file;
  }

  bool openFileForRead(const char*, const char* path, FsFile& file) {
    auto it = HostStorage::files().find(path);
    if (it == HostStorage::files().end()) {
      file.close();
      return false;
    }

    file.data_ = std::make_shared<std::vector<uint8_t>>(it->second);
    file.isDir_ = false;
    file.directoryEntries_.clear();
    file.directoryIndex_ = 0;
    file.path_ = path;
    const size_t slash = file.path_.find_last_of('/');
    file.name_ = (slash == std::string::npos) ? file.path_ : file.path_.substr(slash + 1);
    file.position_ = 0;
    return true;
  }
};
