#pragma once

#include <SDCardManager.h>

class HalStorage {
 public:
  static HalStorage& getInstance() {
    static HalStorage instance;
    return instance;
  }

  bool openFileForRead(const char* moduleName, const char* path, FsFile& file) {
    return SDCardManager::getInstance().openFileForRead(moduleName, path, file);
  }
};

#define Storage HalStorage::getInstance()

inline unsigned long millis() { return 0; }
