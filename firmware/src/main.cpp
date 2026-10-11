#include <Arduino.h>
#include <Preferences.h>

#include "secrets.h"
#include "EpaperDisplay.h"
#include "SleepSchedule.h"
#include "StravaApi.h"
#include "WifiConnect.h"

// Wake up this many seconds after the full hour, so a slightly early wake-up still lands after it
static const uint32_t WAKE_DELAY_S = 5;
// The RTC timer drifts. A timer wake-up less than this before the full hour was too early:
// sleep the rest first, so the display really refreshes at the full hour.
static const uint32_t EARLY_WAKE_MARGIN_S = 600;
// Fallback interval if the clock has never been set (no WiFi since power-on)
static const uint32_t FALLBACK_SLEEP_S = 3600;

// ID of the activity currently on the display, kept in flash (NVS) so it survives deep sleep,
// resets and power loss. 0 means no activity is shown (nothing drawn yet, or an error screen).
static const char *PREFS_NAMESPACE = "display";
static const char *PREFS_SHOWN_ID = "shownId";

static uint64_t loadShownActivityId() {
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return 0;  // namespace doesn't exist yet
    uint64_t id = prefs.getULong64(PREFS_SHOWN_ID, 0);
    prefs.end();
    return id;
}

static void saveShownActivityId(uint64_t id) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putULong64(PREFS_SHOWN_ID, id);
    prefs.end();
}

// Draws an error screen; it replaces the activity, so the next fetch draws it again.
static void showError(const char *message) {
    initDisplay();
    drawError(message);
    hibernateDisplay();
    saveShownActivityId(0);
}

// Loads the latest activity and draws it if it isn't on the display already,
// or draws the error that occurred. Needs WiFi.
static void showLastActivity() {
    String accessToken;
    StravaActivity activity;
    StravaError stravaError =
        fetchAccessToken(STRAVA_CLIENT_ID, STRAVA_CLIENT_SECRET, STRAVA_REFRESH_TOKEN, accessToken);
    if (stravaError.ok()) {
        stravaError = fetchLastActivity(accessToken, activity);
        // stravaError = fetchActivityById(accessToken, 19572817451, activity);
    }
    if (!stravaError.ok()) {
        Serial.printf("Strava error %d (HTTP %d): %s\n%s\n", static_cast<int>(stravaError.code),
                      stravaError.httpStatus, stravaError.message(), stravaError.detail.c_str());
        showError(stravaError.message());
        return;
    }

    Serial.printf("%llu | %s | %s | %s | %.2f km\n", (unsigned long long)activity.id,
                  activity.name.c_str(), activity.type.c_str(), activity.date.c_str(),
                  activity.distance_km);
    if (activity.id == loadShownActivityId()) {
        Serial.println("Activity is already shown, not refreshing the display");
        return;
    }

    initDisplay();
    drawActivity(activity);
    hibernateDisplay();
    saveShownActivityId(activity.id);
}

[[noreturn]] static void sleepUntilNextFullHour() {
    disconnectWifi();
    deepSleepFor(clockIsSet() ? secondsUntilNextFullHour() + WAKE_DELAY_S : FALLBACK_SLEEP_S);
}

void setup() {
    Serial.begin(115200);
    printLocalTime(wokeFromDeepSleep() ? "Woke up at" : "Started at");

    WifiError wifiError = connectWifi(WIFI_SSID, WIFI_PASSWORD);
    if (!wifiError.ok()) {
        Serial.printf("WiFi error %d (status %d): %s\n", static_cast<int>(wifiError.code),
                      wifiError.status, wifiError.message());
        showError(wifiError.message());
        sleepUntilNextFullHour();  // by the RTC clock, if it was set before
    }

    if (!syncClock()) {
        Serial.println("NTP time sync failed, using the RTC clock");
    } else if (wokeFromDeepSleep() && secondsUntilNextFullHour() < EARLY_WAKE_MARGIN_S) {
        printLocalTime("Woke up too early, it's");
        sleepUntilNextFullHour();
    }
    printLocalTime("Refreshing at");

    showLastActivity();
    sleepUntilNextFullHour();
}

void loop() {}  // never reached, setup() always ends in deep sleep
