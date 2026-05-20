#pragma once

#include <SDCardManager.h>

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  FsFile open(const char* path, const oflag_t oflag = O_RDONLY) {
    return SDCardManager::getInstance().open(path, oflag);
  }

  bool mkdir(const char*, const bool = true) { return true; }

  bool openFileForRead(const char* moduleName, const char* path, FsFile& file) {
    return SDCardManager::getInstance().openFileForRead(moduleName, path, file);
  }

  bool openFileForWrite(const char*, const char*, FsFile&) { return false; }
};

#define Storage HalStorage::getInstance()

using HalFile = FsFile;

inline unsigned long millis() { return 0; }
