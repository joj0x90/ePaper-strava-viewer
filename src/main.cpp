#include <Arduino.h>
#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#include "Battery.h"
#include "EpaperDisplay.h"
#include "OtaUpdate.h"
#include "Settings.h"
#include "SetupPortal.h"
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
// How long the setup page is available after a button press, or if the device isn't set up
static const uint8_t SETUP_MINUTES = 10;

// What is currently on the display, kept in flash (NVS) so it survives deep sleep, resets and
// power loss: the activity ID (0 = no activity, e.g. an error screen), the firmware that drew
// it, and what can change about the activity or footer. Flashing a new firmware (e.g. a layout
// change) thus redraws the same activity once.
static const char *PREFS_NAMESPACE = "display";
static const char *PREFS_SHOWN_ID = "shownId";
static const char *PREFS_SHOWN_FIRMWARE = "shownFw";
static const char *PREFS_SHOWN_NOTICE = "shownNotice";
static const char *PREFS_SHOWN_BATTERY = "shownBattery";
static const char *PREFS_SHOWN_BELL = "shownBell";
static const char *PREFS_SHOWN_NAME = "shownName";
static const char *PREFS_SHOWN_DESCRIPTION = "shownDesc";
static const char *PREFS_SHOWN_DATE = "shownDate";
static const char *PREFS_SHOWN_KUDOS = "shownKudos";
static const char *PREFS_SHOWN_COMMENTS = "shownComments";
static const char *PREFS_SHOWN_CHART = "shownChart";
static const char *PREFS_PARTIAL_REFRESHES = "partials";
// The battery charge alone redraws the display only after it changed by this many percent, so
// ADC noise doesn't cost an e-paper refresh (and battery) every hour.
static const int8_t BATTERY_REDRAW_STEP = 5;
// Changes of the shown activity are drawn with a partial refresh, which leaves ghosting behind:
// after this many in a row, the next one is a full refresh.
static const uint8_t MAX_PARTIAL_REFRESHES = 5;
// The description is kept only this long (NVS strings are limited to 4000 bytes). The display
// shows far less of it, so the cut-off part changes nothing on it.
static const size_t SHOWN_DESCRIPTION_LENGTH = 1000;

// The activity screen on the display. activity has only the fields that can change filled.
struct ShownScreen {
    uint64_t activityId = 0;
    String firmware;
    String notice;
    int8_t batteryPercent = -1;
    bool updateBell = false;
    StravaActivity activity;
    bool heartRateChart = false;   // the heart rate request can fail, which leaves the chart out
    uint8_t partialRefreshes = 0;  // since the last full refresh
};

static ShownScreen loadShownScreen() {
    ShownScreen shown;
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return shown;  // namespace doesn't exist yet
    shown.activityId = prefs.getULong64(PREFS_SHOWN_ID, 0);
    shown.firmware = prefs.getString(PREFS_SHOWN_FIRMWARE);
    shown.notice = prefs.getString(PREFS_SHOWN_NOTICE);
    shown.batteryPercent = prefs.getChar(PREFS_SHOWN_BATTERY, -1);
    shown.updateBell = prefs.getBool(PREFS_SHOWN_BELL, false);
    shown.activity.name = prefs.getString(PREFS_SHOWN_NAME);
    shown.activity.description = prefs.getString(PREFS_SHOWN_DESCRIPTION);
    shown.activity.date = prefs.getString(PREFS_SHOWN_DATE);
    shown.activity.kudos_count = prefs.getUChar(PREFS_SHOWN_KUDOS, 0);
    shown.activity.comment_count = prefs.getUChar(PREFS_SHOWN_COMMENTS, 0);
    shown.heartRateChart = prefs.getBool(PREFS_SHOWN_CHART, false);
    shown.partialRefreshes = prefs.getUChar(PREFS_PARTIAL_REFRESHES, 0);
    prefs.end();
    return shown;
}

// True if the activity's fields that can change are the shown ones
static bool sameActivityFields(const StravaActivity &shown, const StravaActivity &activity) {
    return shown.name == activity.name &&
           shown.description == activity.description.substring(0, SHOWN_DESCRIPTION_LENGTH) &&
           shown.date == activity.date && shown.kudos_count == activity.kudos_count &&
           shown.comment_count == activity.comment_count;
}

static bool batteryIsShown(int8_t shownPercent, int8_t batteryPercent) {
    // a battery that was connected or disconnected (-1) always counts as a change
    return (shownPercent < 0) == (batteryPercent < 0) &&
           abs(shownPercent - batteryPercent) < BATTERY_REDRAW_STEP;
}

static void saveShownActivityId(uint64_t id) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putULong64(PREFS_SHOWN_ID, id);
    prefs.putString(PREFS_SHOWN_FIRMWARE, firmwareHash());
    prefs.end();
}

static void saveShownActivity(const StravaActivity &activity, const String &notice,
                              int8_t batteryPercent, bool updateBell, uint8_t partialRefreshes) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putULong64(PREFS_SHOWN_ID, activity.id);
    prefs.putString(PREFS_SHOWN_FIRMWARE, firmwareHash());
    prefs.putString(PREFS_SHOWN_NOTICE, notice);
    prefs.putChar(PREFS_SHOWN_BATTERY, batteryPercent);
    prefs.putBool(PREFS_SHOWN_BELL, updateBell);
    prefs.putString(PREFS_SHOWN_NAME, activity.name);
    prefs.putString(PREFS_SHOWN_DESCRIPTION,
                    activity.description.substring(0, SHOWN_DESCRIPTION_LENGTH));
    prefs.putString(PREFS_SHOWN_DATE, activity.date);
    prefs.putUChar(PREFS_SHOWN_KUDOS, activity.kudos_count);
    prefs.putUChar(PREFS_SHOWN_COMMENTS, activity.comment_count);
    prefs.putBool(PREFS_SHOWN_CHART, activity.heart_rate.count > 0);
    prefs.putUChar(PREFS_PARTIAL_REFRESHES, partialRefreshes);
    prefs.end();
}

// Draws an error screen; it replaces the activity, so the next fetch draws it again.
static void showError(const char *message) {
    initDisplay();
    drawError(message);
    hibernateDisplay();
    saveShownActivityId(0);
}

// Error the settings can fix, e.g. a WiFi that isn't found: also tells how to open the setup page.
static void showSetupHint(const char *heading, const char *message, SetupHint hint) {
    initDisplay();
    drawSetupHint(heading, message, hint, accessPointSsid().c_str(), accessPointPassword().c_str(),
                  SETUP_AP_URL, SETUP_MINUTES);
    hibernateDisplay();
    saveShownActivityId(0);
}

static bool hasStravaCredentials(const Credentials &c) {
    return !c.stravaClientId.isEmpty() && !c.stravaClientSecret.isEmpty() &&
           !c.stravaRefreshToken.isEmpty();
}

// 4xx from the token endpoint: the Strava credentials are wrong. 401 for the activities: the
// login works, but wasn't allowed to read activities. Connecting Strava again fixes both.
static bool isLoginRejected(const StravaError &error) {
    if (error.code == StravaErrorCode::ActivityRequestFailed) return error.httpStatus == 401;
    return error.code == StravaErrorCode::TokenRequestFailed && error.httpStatus >= 400 &&
           error.httpStatus < 500;
}

// What the display says about a rejected login
static const char *loginRejectedMessage(const StravaError &error) {
    return error.code == StravaErrorCode::ActivityRequestFailed ? "Strava permission missing"
                                                                : error.message();
}

// Tells the user why the activity takes longer; it replaces the activity, so the activity is
// drawn again if the update fails.
static void showUpdating(const char *newVersion) {
    initDisplay();
    drawUpdate(FIRMWARE_VERSION, newVersion);
    hibernateDisplay();
    saveShownActivityId(0);
}

// Loads the latest activity and draws it with the footer notice, battery charge and update bell
// if it isn't on the display already, or draws the error that occurred. If only the activity's
// title, description, date, kudos or comments or the footer changed, it's redrawn with a partial
// refresh. A rejected login is only returned: the caller decides how to get new credentials.
// Needs WiFi.
static StravaError showLastActivity(const Credentials &credentials, const String &notice,
                                    int8_t batteryPercent, bool updateBell) {
    String accessToken;
    StravaActivity activity;
    StravaError stravaError =
        fetchAccessToken(credentials.stravaClientId.c_str(), credentials.stravaClientSecret.c_str(),
                         credentials.stravaRefreshToken.c_str(), accessToken);
    if (stravaError.ok()) {
        stravaError = fetchLastActivity(accessToken, activity);
        // the list has no description, so its changes are only seen in the details
        if (stravaError.ok()) stravaError = fetchActivityById(accessToken, activity.id, activity);
        // stravaError = fetchActivityById(accessToken, 20351131615, activity);
    }
    if (!stravaError.ok()) {
        Serial.printf("Strava error %d (HTTP %d): %s\n%s\n", static_cast<int>(stravaError.code),
                      stravaError.httpStatus, stravaError.message(), stravaError.detail.c_str());
        if (!isLoginRejected(stravaError)) showError(stravaError.message());
        return stravaError;
    }

    Serial.printf("%llu | %s | %s | %s | %.2f km\n", (unsigned long long)activity.id,
                  activity.name.c_str(), activity.type.c_str(), activity.date.c_str(),
                  activity.distance_km);
    const ShownScreen shown = loadShownScreen();
    // a partial refresh draws the shown screen again (with the stored fields), which only gives
    // the same pixels for the same activity drawn by the same firmware
    const bool sameActivity = shown.activityId == activity.id && shown.firmware == firmwareHash();
    const bool sameFields = sameActivityFields(shown.activity, activity);
    const bool sameFooter = shown.notice == notice &&
                            batteryIsShown(shown.batteryPercent, batteryPercent) &&
                            shown.updateBell == updateBell;
    if (sameActivity && sameFields && sameFooter) {
        Serial.println("Activity, battery and bell are already shown by this firmware, "
                       "not refreshing");
        return stravaError;
    }
    // the chart takes the map's place, so it's only loaded (one more request) without a map
    if (!activity.showMap && activity.has_heartrate) {
        const StravaError heartRateError = fetchHeartRate(accessToken, activity);
        if (!heartRateError.ok()) {  // not worth an error screen: drawn without the chart
            Serial.printf("Heart rate error %d (HTTP %d): %s\n%s\n",
                          static_cast<int>(heartRateError.code), heartRateError.httpStatus,
                          heartRateError.message(), heartRateError.detail.c_str());
        }
    }

    const bool sameChart = shown.heartRateChart == (activity.heart_rate.count > 0);
    if (sameActivity && sameChart && shown.partialRefreshes < MAX_PARTIAL_REFRESHES) {
        Serial.println("Activity or footer changed, partial refresh");
        // the shown screen is the new activity with the stored fields that can change
        StravaActivity shownActivity = activity;
        shownActivity.name = shown.activity.name;
        shownActivity.description = shown.activity.description;
        shownActivity.date = shown.activity.date;
        shownActivity.kudos_count = shown.activity.kudos_count;
        shownActivity.comment_count = shown.activity.comment_count;
        initDisplay(true);
        drawActivityChanges(shownActivity, shown.notice.c_str(), shown.batteryPercent,
                            shown.updateBell, activity, notice.c_str(), batteryPercent, updateBell);
        hibernateDisplay();
        saveShownActivity(activity, notice, batteryPercent, updateBell, shown.partialRefreshes + 1);
        return stravaError;
    }

    initDisplay();
    drawActivity(activity, notice.c_str(), batteryPercent, updateBell);
    hibernateDisplay();
    saveShownActivity(activity, notice, batteryPercent, updateBell, 0);
    return stravaError;
}

[[noreturn]] static void sleepUntilNextFullHour() {
    disconnectWifi();
    deepSleepFor(clockIsSet() ? secondsUntilNextFullHour() + WAKE_DELAY_S : FALLBACK_SLEEP_S);
}

[[noreturn]] static void showActivityAndSleep(const Credentials &credentials,
                                              const DeviceStatus &status, bool mayOpenSetup,
                                              const String &updateError = "");

// Opens the setup page for SETUP_MINUTES: in the WiFi if it's connected or connects, otherwise in
// the device's own access point, and shows how to reach it, with the message (why) if there is
// one. Saving or resetting restarts the device with the new settings; installing an update
// restarts it with the new firmware, or shows the activity with the error. When the time is up, it
// continues with the settings it has, without opening the setup page again, so missing
// credentials can't keep it in setup mode; without WiFi credentials it sleeps until the button.
[[noreturn]] static void runSetupMode(const Credentials &credentials, const DeviceStatus &status,
                                      const char *message = "") {
    const bool inWifi =
        WiFi.isConnected() ||
        (!credentials.wifiSsid.isEmpty() &&
         connectWifi(credentials.wifiSsid.c_str(), credentials.wifiPassword.c_str()).ok());
    // so the setup page offers a release that came out since the last hourly check
    if (inWifi) checkForUpdate();
    const String url = startSetupPortal(!inWifi, status, SETUP_MINUTES * 60000UL);
    Serial.printf("Setup mode for %u minutes: %s\n", SETUP_MINUTES, url.c_str());

    initDisplay();
    if (inWifi) {
        drawSetupInWifi(url.c_str(), SETUP_MDNS_URL, credentials.wifiSsid.c_str(), message,
                        SETUP_MINUTES);
    } else {
        drawSetupAccessPoint(accessPointSsid().c_str(), accessPointPassword().c_str(), url.c_str(),
                             SETUP_MINUTES);
    }
    hibernateDisplay();
    saveShownActivityId(0);

    const SetupResult result = runSetupPortal();
    if (result == SetupResult::Update) {
        // the setup page offers it only in the WiFi, which the portal switched off afterwards
        String error = "WiFi failed";
        if (connectWifi(credentials.wifiSsid.c_str(), credentials.wifiPassword.c_str()).ok()) {
            error = installUpdate(availableUpdate(), showUpdating);  // restarts on success
        }
        showActivityAndSleep(credentials, status, false, "Update failed: " + error);
    }
    if (result != SetupResult::TimedOut) esp_restart();

    // nothing was saved, so the credentials are still the same
    if (credentials.wifiSsid.isEmpty()) {
        showSetupHint("Not set up", "The setup mode ended without saved settings.",
                      SetupHint::NotSetUp);
        deepSleepUntilButton();  // the timer would only open the setup page again and again
    }
    showActivityAndSleep(credentials, status, false);
}

// Connects to the WiFi, checks for a new release (shown as a bell, installed from the setup
// page), shows the latest activity and sleeps until the next full hour. With mayOpenSetup,
// missing or rejected Strava credentials open the setup page in the WiFi; otherwise they're only
// shown, so a broken login doesn't keep the device in setup mode every hour. updateError, from an
// update that failed right before, is shown in the footer instead of the check's error.
[[noreturn]] static void showActivityAndSleep(const Credentials &credentials,
                                              const DeviceStatus &status, bool mayOpenSetup,
                                              const String &updateError) {
    WifiError wifiError =
        connectWifi(credentials.wifiSsid.c_str(), credentials.wifiPassword.c_str());
    if (!wifiError.ok()) {
        Serial.printf("WiFi error %d (status %d): %s\n", static_cast<int>(wifiError.code),
                      wifiError.status, wifiError.message());
        showSetupHint("Error:", wifiError.message(), SetupHint::WifiFailed);
        sleepUntilNextFullHour();  // by the RTC clock, if it was set before
    }

    if (!syncClock()) {
        Serial.println("NTP time sync failed, using the RTC clock");
    } else if (wokeFromDeepSleep() && secondsUntilNextFullHour() < EARLY_WAKE_MARGIN_S) {
        printLocalTime("Woke up too early, it's");
        sleepUntilNextFullHour();
    }
    const String checkError = checkForUpdate();
    String notice = updateError;
    if (notice.isEmpty() && !checkError.isEmpty()) notice = "Update check failed: " + checkError;
    const bool updateBell = !availableUpdate().isEmpty();

    if (!hasStravaCredentials(credentials)) {
        const char *message = "Strava isn't set up yet";
        Serial.println(message);
        if (mayOpenSetup) runSetupMode(credentials, status, message);
        showSetupHint("Error:", message, SetupHint::StravaFailed);
        disconnectWifi();
        deepSleepUntilButton();  // there's nothing to load until it's set up
    }

    printLocalTime("Refreshing at");
    const StravaError stravaError =
        showLastActivity(credentials, notice, status.batteryPercent, updateBell);
    if (isLoginRejected(stravaError)) {
        if (mayOpenSetup) runSetupMode(credentials, status, loginRejectedMessage(stravaError));
        showSetupHint("Error:", loginRejectedMessage(stravaError), SetupHint::StravaFailed);
    }
    sleepUntilNextFullHour();
}

void setup() {
    Serial.begin(115200);
    printLocalTime(wokeFromDeepSleep() ? "Woke up at" : "Started at");

    // before WiFi, whose current peaks pull the battery voltage down
    const float batteryVolts = readBatteryVoltage();
    const int8_t batteryPercent = ::batteryPercent(batteryVolts);
    Serial.printf("Battery: %.2f V, %d %%\n", batteryVolts, batteryPercent);
    const DeviceStatus status = {batteryVolts, batteryPercent};

#if __has_include("secrets.h")
    provisionCredentials(
        {WIFI_SSID, WIFI_PASSWORD, STRAVA_CLIENT_ID, STRAVA_CLIENT_SECRET, STRAVA_REFRESH_TOKEN});
#endif
    const Credentials credentials = loadCredentials();
    if (credentials.wifiSsid.isEmpty()) {
        Serial.println("Not set up yet");
        runSetupMode(credentials, status);
    }
    if (wokeByButton()) {
        Serial.println("Woken up by the button");
        runSetupMode(credentials, status);
    }
    // a start by the user (power-on, restart after saving) may open the setup page, the hourly
    // timer doesn't
    showActivityAndSleep(credentials, status, !wokeFromDeepSleep());
}

void loop() {}  // never reached, setup() always ends in deep sleep
