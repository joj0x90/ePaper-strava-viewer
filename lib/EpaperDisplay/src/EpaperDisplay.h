#ifndef EPAPER_DISPLAY_H
#define EPAPER_DISPLAY_H

#include <Arduino.h>

#include "StravaApi.h"

// Sets up SPI and the 7.5" e-paper panel. Call once before drawing. keepScreen is for
// drawActivityChanges(): the panel still shows what was drawn before the last hibernateDisplay().
void initDisplay(bool keepScreen = false);

// footerNotice is shown next to the firmware version, e.g. a failed update. batteryPercent is
// shown in the footer's right corner, unless it's negative (no battery). updateAvailable shows a
// bell in the footer's left corner.
void drawActivity(const StravaActivity &activity, const char *footerNotice = "",
                  int8_t batteryPercent = -1, bool updateAvailable = false);
// Like drawActivity(), but with a fast partial refresh that doesn't flash the screen: only the
// pixels that differ from the shown screen change. shown and its footer must be exactly what
// drawActivity() or drawActivityChanges() drew last, e.g. the same activity with fewer kudos.
// Partial refreshes leave ghosting behind, so do a full one with drawActivity() now and then.
void drawActivityChanges(const StravaActivity &shown, const char *shownNotice,
                         int8_t shownBatteryPercent, bool shownUpdateAvailable,
                         const StravaActivity &activity, const char *footerNotice,
                         int8_t batteryPercent, bool updateAvailable);
void drawError(const char *message);
// Shown while a new firmware is downloaded, which delays the activity.
void drawUpdate(const char *fromVersion, const char *toVersion);

// Setup page in the device's own WiFi: a QR code to connect to it and one to open url.
void drawSetupAccessPoint(const char *apSsid, const char *apPassword, const char *url,
                          uint8_t minutes);
// Setup page in the normal WiFi wifiSsid: a QR code to open url, mdnsUrl as an alternative.
// message (e.g. "Strava login failed") replaces the heading "Setup" if it isn't empty.
void drawSetupInWifi(const char *url, const char *mdnsUrl, const char *wifiSsid,
                     const char *message, uint8_t minutes);

// What drawSetupHint() explains
enum class SetupHint : uint8_t {
    NotSetUp,      // no WiFi credentials: the button opens the device's own WiFi
    WifiFailed,    // the button opens the device's own WiFi if the normal one still fails
    StravaFailed,  // the WiFi works: the button opens the setup page in it, no own WiFi needed
};

// Error the settings can fix (or that there are none), with how to open the setup page: the
// button and, unless the WiFi works, a QR code to connect to the device's own WiFi.
void drawSetupHint(const char *heading, const char *message, SetupHint hint, const char *apSsid,
                   const char *apPassword, const char *apUrl, uint8_t minutes);

// Test screen with every sport type, its icon and its name.
void drawIconOverviewScreen();

// Puts the panel into deep sleep; call initDisplay() again before the next draw.
void hibernateDisplay();

#endif  // EPAPER_DISPLAY_H
