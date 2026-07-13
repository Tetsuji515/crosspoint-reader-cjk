#include "MemLog.h"

#include <Logging.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

namespace MemLog {

void log(const char* phase) {
  const uint32_t freeHeap = esp_get_free_heap_size();
  const uint32_t minFree = esp_get_minimum_free_heap_size();
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  LOG_INF("MEM", "%s,%u,%u,%u,%u", phase, freeHeap, minFree, static_cast<uint32_t>(largest),
          static_cast<uint32_t>(internal));
}

}  // namespace MemLog
