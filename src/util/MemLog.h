#pragma once

// Heap measurement helper for the low-memory crash investigation
// (docs/dev-notes/mem-investigation.md, feature/mem-investigation branch).
// Measurement only -- emits a single CSV-style [MEM] log line per call and
// changes no behavior. Remove once the investigation concludes.
//
// Output format (parseable by scripts/debugging_monitor.py):
//   [MEM] phase,free,minfree,largest,internal
// where:
//   free     = esp_get_free_heap_size()                         (total free)
//   minfree  = esp_get_minimum_free_heap_size()                 (min since boot)
//   largest  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) (fragmentation)
//   internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL)     (internal DRAM)
namespace MemLog {
void log(const char* phase);
}  // namespace MemLog
