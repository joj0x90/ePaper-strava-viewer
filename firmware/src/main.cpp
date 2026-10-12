#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "EpaperDisplay.h"
#include "OtaUpdate.h"
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
static const char *PREFS_SHOWN_NOTICE = "shownNotice";

// WiFi and Strava credentials, kept in NVS: the release builds from CI are public and contain
// none. A local build with include/secrets.h stores them on boot, so flash one once via USB.
static const char *PREFS_CREDENTIALS = "credentials";

struct Credentials {
    String wifiSsid, wifiPassword;
    String stravaClientId, stravaClientSecret, stravaRefreshToken;
};

static Credentials loadCredentials() {
    Preferences prefs;
#if __has_include("secrets.h")
    prefs.begin(PREFS_CREDENTIALS, false);
    prefs.putString("wifiSsid", WIFI_SSID);
    prefs.putString("wifiPassword", WIFI_PASSWORD);
    prefs.putString("clientId", STRAVA_CLIENT_ID);
    prefs.putString("clientSecret", STRAVA_CLIENT_SECRET);
    prefs.putString("refreshToken", STRAVA_REFRESH_TOKEN);
#else
    prefs.begin(PREFS_CREDENTIALS, true);  // fails if never provisioned: all strings stay empty
#endif
    Credentials c;
    c.wifiSsid = prefs.getString("wifiSsid");
    c.wifiPassword = prefs.getString("wifiPassword");
    c.stravaClientId = prefs.getString("clientId");
    c.stravaClientSecret = prefs.getString("clientSecret");
    c.stravaRefreshToken = prefs.getString("refreshToken");
    prefs.end();
    return c;
}

// Hash of the running firmware, changes with every build that changes the code
static String firmwareHash() {
    char hash[17];  // the first 16 hex digits are enough to tell builds apart
    esp_ota_get_app_elf_sha256(hash, sizeof(hash));
    return hash;
}

static bool isAlreadyShown(uint64_t activityId, const String &notice) {
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return false;  // namespace doesn't exist yet
    bool shown = prefs.getULong64(PREFS_SHOWN_ID, 0) == activityId &&
                 prefs.getString(PREFS_SHOWN_FIRMWARE) == firmwareHash() &&
                 prefs.getString(PREFS_SHOWN_NOTICE) == notice;
    prefs.end();
    return shown;
}

static void saveShownActivityId(uint64_t id, const String &notice = "") {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putULong64(PREFS_SHOWN_ID, id);
    prefs.putString(PREFS_SHOWN_FIRMWARE, firmwareHash());
    prefs.putString(PREFS_SHOWN_NOTICE, notice);
    prefs.end();
}

// Draws an error screen; it replaces the activity, so the next fetch draws it again.
static void showError(const char *message) {
    initDisplay();
    drawError(message);
    hibernateDisplay();
    saveShownActivityId(0);
}

// Tells the user why the activity takes longer; it replaces the activity, so the activity is
// drawn again if the update fails.
static void showUpdating(const char *newVersion) {
    initDisplay();
    drawUpdate(FIRMWARE_VERSION, newVersion);
    hibernateDisplay();
    saveShownActivityId(0);
}

// Loads the latest activity and draws it with the footer notice if it isn't on the display
// already, or draws the error that occurred. Needs WiFi.
static void showLastActivity(const Credentials &credentials, const String &notice) {
    String accessToken;
    StravaActivity activity;
    StravaError stravaError =
        fetchAccessToken(credentials.stravaClientId.c_str(), credentials.stravaClientSecret.c_str(),
                         credentials.stravaRefreshToken.c_str(), accessToken);
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
    if (isAlreadyShown(activity.id, notice)) {
        Serial.println("Activity is already shown by this firmware, not refreshing the display");
        return;
    }

    initDisplay();
    drawActivity(activity, notice.c_str());
    hibernateDisplay();
    saveShownActivityId(activity.id, notice);
}

[[noreturn]] static void sleepUntilNextFullHour() {
    disconnectWifi();
    deepSleepFor(clockIsSet() ? secondsUntilNextFullHour() + WAKE_DELAY_S : FALLBACK_SLEEP_S);
}

void setup() {
    Serial.begin(115200);
    printLocalTime(wokeFromDeepSleep() ? "Woke up at" : "Started at");

    const Credentials credentials = loadCredentials();
    if (credentials.wifiSsid.isEmpty()) {
        Serial.println("No credentials in NVS, flash a build with include/secrets.h once via USB");
        showError("No credentials, flash once via USB");
        sleepUntilNextFullHour();
    }

    WifiError wifiError =
        connectWifi(credentials.wifiSsid.c_str(), credentials.wifiPassword.c_str());
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
    // before the display, so a release that breaks drawing can still be replaced
    const String updateError = updateFirmwareIfAvailable(showUpdating);
    const String notice = updateError.isEmpty() ? "" : "Update failed: " + updateError;

    printLocalTime("Refreshing at");

    showLastActivity(credentials, notice);
    sleepUntilNextFullHour();
}

void loop() {}  // never reached, setup() always ends in deep sleep
