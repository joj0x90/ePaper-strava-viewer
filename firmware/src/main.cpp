#include <Arduino.h>

#include "secrets.h"
#include "EpaperDisplay.h"
#include "StravaApi.h"
#include "WifiConnect.h"

// Loads the latest activity and draws it, or draws the first error that occurs.
static void showLastActivity() {
    WifiError wifiError = connectWifi(WIFI_SSID, WIFI_PASSWORD);
    if (!wifiError.ok()) {
        Serial.printf("WiFi error %d (status %d): %s\n", static_cast<int>(wifiError.code),
                      wifiError.status, wifiError.message());
        drawError(wifiError.message());
        return;
    }

    String accessToken;
    StravaActivity activity;
    StravaError stravaError =
        fetchAccessToken(STRAVA_CLIENT_ID, STRAVA_CLIENT_SECRET, STRAVA_REFRESH_TOKEN, accessToken);
    if (stravaError.ok()) {
        stravaError = fetchLastActivity(accessToken, activity);
    }
    if (!stravaError.ok()) {
        Serial.printf("Strava error %d (HTTP %d): %s\n%s\n", static_cast<int>(stravaError.code),
                      stravaError.httpStatus, stravaError.message(), stravaError.detail.c_str());
        drawError(stravaError.message());
        return;
    }

    Serial.printf("%s | %s | %.2f km\n", activity.type.c_str(), activity.date.c_str(),
                  activity.distance_km);
    drawActivity(activity);
}

void setup() {
    Serial.begin(115200);

    initDisplay();
    showLastActivity();

    disconnectWifi();
    hibernateDisplay();
}

void loop() {}
