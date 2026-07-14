#include "ClockSync.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <sys/time.h>

#include "NtpSync.h"

namespace {
constexpr char CLOCK_DIR[] = "/.apps";
constexpr char CLOCK_FILE[] = "/.apps/clock.json";

time_t readLastNtpSyncEpoch() {
  if (!Storage.exists(CLOCK_FILE)) {
    return 0;
  }
  String existing = Storage.readFile(CLOCK_FILE);
  if (existing.isEmpty()) {
    return 0;
  }
  JsonDocument doc;
  if (deserializeJson(doc, existing.c_str())) {
    return 0;
  }
  return doc["lastNtpSyncEpoch"] | static_cast<time_t>(0);
}

bool writeState(time_t lastKnownEpoch, time_t lastNtpSyncEpoch) {
  Storage.mkdir(CLOCK_DIR);
  JsonDocument doc;
  doc["lastKnownEpoch"] = lastKnownEpoch;
  doc["lastNtpSyncEpoch"] = lastNtpSyncEpoch;
  doc["tzOffsetMinutes"] = ClockSync::TZ_OFFSET_MINUTES;
  String json;
  serializeJson(doc, json);
  return Storage.writeFile(CLOCK_FILE, json);
}
}  // namespace

namespace ClockSync {

bool isTimeSynced() { return time(nullptr) >= UNSYNCED_EPOCH_THRESHOLD; }

void restoreApproximateTimeFromDisk() {
  if (!Storage.exists(CLOCK_FILE)) {
    return;
  }
  String json = Storage.readFile(CLOCK_FILE);
  if (json.isEmpty()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, json.c_str())) {
    LOG_ERR("CLOCK", "Failed to parse %s", CLOCK_FILE);
    return;
  }
  const time_t lastKnownEpoch = doc["lastKnownEpoch"] | static_cast<time_t>(0);
  if (lastKnownEpoch < UNSYNCED_EPOCH_THRESHOLD) {
    return;
  }
  struct timeval tv = {.tv_sec = lastKnownEpoch, .tv_usec = 0};
  settimeofday(&tv, nullptr);
  LOG_DBG("CLOCK", "Restored approximate time from disk (epoch %ld)", static_cast<long>(lastKnownEpoch));
}

void saveCurrentTimeToDisk() {
  const time_t now = time(nullptr);
  if (now < UNSYNCED_EPOCH_THRESHOLD) {
    return;  // Nothing valid to persist
  }
  writeState(now, readLastNtpSyncEpoch());
}

bool syncAndPersist() {
  syncTimeWithNTP();
  if (!isTimeSynced()) {
    LOG_DBG("CLOCK", "syncAndPersist: NTP sync did not produce a valid time");
    return false;
  }
  const time_t now = time(nullptr);
  const bool saved = writeState(now, now);
  LOG_DBG("CLOCK", "syncAndPersist: NTP synced and %s", saved ? "persisted" : "persist FAILED");
  return saved;
}

}  // namespace ClockSync
