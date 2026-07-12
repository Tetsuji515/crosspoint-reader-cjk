#pragma once

// Synchronously configures SNTP (pool.ntp.org) and blocks for up to ~5 seconds
// waiting for a time sync. Caller must already have an active WiFi connection.
// Shared by KOReaderSyncActivity and the home launcher's clock area.
void syncTimeWithNTP();
