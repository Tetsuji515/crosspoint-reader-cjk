#pragma once

#include <cstddef>
#include <cstdint>

#include "I18n.h"
#include "activities/ActivityManager.h"

// Function pointer (not std::function) so AppEntry stays a trivial, flash-resident
// POD: no vtable, no heap-allocated closure. Appropriate for a 380KB-RAM device.
using LaunchFn = void (*)();

struct AppEntry {
  StrId nameKey;         // i18n key for the app's display name
  const uint8_t* icon;   // 1-bit bitmap for GfxRenderer::drawIcon(); nullptr => placeholder box + initial letter
  bool enabled;           // false => selecting the entry shows a "coming soon" toast instead of launching
  LaunchFn create;        // nullptr when enabled == false
};

inline void launchBookshelf() { activityManager.goToBookshelf(); }
inline void launchFileTransfer() { activityManager.goToFileTransfer(); }
inline void launchSettings() { activityManager.goToSettings(); }

// Registration order == display order in the launcher list.
inline constexpr AppEntry APP_REGISTRY[] = {
    {StrId::STR_APP_BOOKSHELF, nullptr, true, &launchBookshelf},
    {StrId::STR_APP_NOTES, nullptr, false, nullptr},
    {StrId::STR_APP_CLOCK_TIMER, nullptr, false, nullptr},
    {StrId::STR_APP_WEATHER, nullptr, false, nullptr},
    {StrId::STR_APP_HABIT_TRACKER, nullptr, false, nullptr},
    {StrId::STR_APP_SLIDESHOW, nullptr, false, nullptr},
    {StrId::STR_APP_SUDOKU, nullptr, false, nullptr},
    {StrId::STR_APP_VAULT_SYNC, nullptr, false, nullptr},
    {StrId::STR_APP_FILE_TRANSFER, nullptr, true, &launchFileTransfer},
    {StrId::STR_APP_SETTINGS, nullptr, true, &launchSettings},
};

inline constexpr int APP_REGISTRY_COUNT = sizeof(APP_REGISTRY) / sizeof(APP_REGISTRY[0]);
