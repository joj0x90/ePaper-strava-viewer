#include <Arduino.h>

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

// Loads the latest activity and draws it, or draws the error that occurred. Needs WiFi.
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
        drawError(stravaError.message());
        return;
    }

    Serial.printf("%s | %s | %s | %.2f km\n", activity.name.c_str(), activity.type.c_str(),
                  activity.date.c_str(), activity.distance_km);
    drawActivity(activity);
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
        initDisplay();
        drawError(wifiError.message());
        hibernateDisplay();
        sleepUntilNextFullHour();  // by the RTC clock, if it was set before
    }

    if (!syncClock()) {
        Serial.println("NTP time sync failed, using the RTC clock");
    } else if (wokeFromDeepSleep() && secondsUntilNextFullHour() < EARLY_WAKE_MARGIN_S) {
        printLocalTime("Woke up too early, it's");
        sleepUntilNextFullHour();
    }
    printLocalTime("Refreshing at");

    initDisplay();
    showLastActivity();
    hibernateDisplay();
    sleepUntilNextFullHour();
}

void loop() {}  // never reached, setup() always ends in deep sleep
