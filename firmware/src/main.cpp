#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

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

// What is currently on the display, kept in flash (NVS) so it survives deep sleep, resets and
// power loss: the activity ID (0 = no activity, e.g. an error screen) and the firmware that drew
// it. Flashing a new firmware (e.g. a layout change) thus redraws the same activity once.
static const char *PREFS_NAMESPACE = "display";
static const char *PREFS_SHOWN_ID = "shownId";
static const char *PREFS_SHOWN_FIRMWARE = "shownFw";

// Hash of the running firmware, changes with every build that changes the code
static String firmwareHash() {
    char hash[17];  // the first 16 hex digits are enough to tell builds apart
    esp_ota_get_app_elf_sha256(hash, sizeof(hash));
    return hash;
}

static bool isAlreadyShown(uint64_t activityId) {
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return false;  // namespace doesn't exist yet
    bool shown = prefs.getULong64(PREFS_SHOWN_ID, 0) == activityId &&
                 prefs.getString(PREFS_SHOWN_FIRMWARE) == firmwareHash();
    prefs.end();
    return shown;
}

static void saveShownActivityId(uint64_t id) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putULong64(PREFS_SHOWN_ID, id);
    prefs.putString(PREFS_SHOWN_FIRMWARE, firmwareHash());
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
        // stravaError = fetchLastActivity(accessToken, activity);
        // Marathon -> 20448162500
        // Kiel-Fahrt -> 19572817451
        // Marathon-Pace-Laf mit Doro -> 20351131615
        // Fahrrad von Dresden -> 20255126553
        // Schwimmkurs 03 -> 20301115286
        stravaError = fetchActivityById(accessToken, 20448162500, activity);
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
    if (isAlreadyShown(activity.id)) {
        Serial.println("Activity is already shown by this firmware, not refreshing the display");
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
