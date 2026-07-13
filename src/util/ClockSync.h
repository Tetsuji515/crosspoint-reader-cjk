#pragma once

#include <ctime>

// Time-source management for the home launcher's clock area.
// See docs/dev-notes/clock-sync-survey.md (STEP C-1/C-2) for the design
// rationale: this device fully powers off (including RTC memory) during
// sleep, so elapsed-sleep-time correction is not possible. Instead, the last
// known epoch is persisted to SD and restored (approximately) at boot, and
// refreshed opportunistically whenever WiFi is connected for any reason.
namespace ClockSync {

// 2020-01-01T00:00:00Z. Below this, the system clock is treated as unset.
constexpr time_t UNSYNCED_EPOCH_THRESHOLD = 1577836800;

// Fixed JST (UTC+9) offset for display. Not yet user-configurable (kept as a
// named constant per the instruction doc rather than hardcoded inline).
constexpr int TZ_OFFSET_MINUTES = 540;

bool isTimeSynced();

// Restores an approximate system time from the last value persisted to SD.
// Call once at boot, before any UI is shown and before the Quick Resume
// decision (main.cpp). Safe no-op if no saved state exists yet.
void restoreApproximateTimeFromDisk();

// Persists the current system time as the "last known" epoch. Call right
// before entering deep sleep (main.cpp's enterDeepSleep()). No-op if the
// current clock looks unsynced (nothing useful to save).
void saveCurrentTimeToDisk();

// Blocking (~5s max): performs an NTP sync (assumes WiFi is already
// connected -- does not manage the WiFi connection itself) and, on success,
// persists the result to SD as both the last-known and last-NTP-synced
// epoch. Returns true on success.
bool syncAndPersist();

}  // namespace ClockSync
