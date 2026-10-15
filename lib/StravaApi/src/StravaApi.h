#ifndef STRAVA_API_H
#define STRAVA_API_H

#include <Arduino.h>

// Number of bars of the heart rate chart
#define HEART_RATE_BARS 60

// Heart rate over the course of the activity, from fetchHeartRate(): the average of each of
// count equal parts of the recorded samples. Activities with fewer samples than
// HEART_RATE_BARS get one bar per sample.
struct HeartRateProfile {
    uint8_t bpm[HEART_RATE_BARS];  // 0 = no valid sample in this part, e.g. a sensor dropout
    uint8_t count = 0;             // number of bars, 0 = not loaded
};

struct StravaActivity {
    uint64_t id;         // Strava activity ID, the number in strava.com/activities/<id>
    String type;         // e.g. "Trail Run"
    String name;         // e.g. "Morning Run"
    String description;  // e.g. "Felt great, sunny weather"
    String date;         // e.g. "09.10.2026 07:12"
    float distance_km;
    bool showMap;  // true for most activities, like runs and rides, false for e.g. indoor swims.
    String map;    // string representation of the map-object
    uint32_t moving_time_s;     // in seconds
    float total_elev_m;         // total elevation in meters
    uint8_t achievement_count;  // number or achievements earned during the activity
    uint8_t kudos_count;        // number of kudos (likes) received for the activity
    uint8_t comment_count;      // number of comments received for the activity
    String device_name;         // name of the device used to record the activity.
    float average_speed;
    float max_speed;
    float burned_calories;
    bool has_heartrate;       // recorded with a heart rate sensor
    float average_heartrate;  // in bpm, 0 without sensor
    float max_heartrate;      // in bpm, 0 without sensor
    HeartRateProfile heart_rate;  // only filled by fetchHeartRate()
};

enum class StravaErrorCode : uint8_t {
    None = 0,
    TokenRequestFailed,  // token endpoint did not answer with HTTP 200
    TokenInvalidJson,
    TokenMissing,           // response had no access_token
    ActivityRequestFailed,  // activities endpoint did not answer with HTTP 200
    ActivityInvalidJson,
    NoActivities,
    ActivityNotFound,  // no activity with that ID, or it belongs to another athlete
};

struct StravaError {
    StravaErrorCode code = StravaErrorCode::None;
    int httpStatus = 0;  // HTTP status, or negative HTTPClient error (e.g. -1 = connection refused)
    String detail;       // response body or JSON parser error, for debugging

    bool ok() const { return code == StravaErrorCode::None; }
    const char *message() const;
};

// Exchanges the long-lived refresh token for a short-lived access token.
StravaError fetchAccessToken(const char *clientId, const char *clientSecret,
                             const char *refreshToken, String &accessToken);

// Exchanges the code from Strava's authorization page (OAuth, see the setup page) for the
// long-lived refresh token, and the athlete's name to confirm whose account it is.
StravaError exchangeAuthorizationCode(const char *clientId, const char *clientSecret,
                                      const char *code, String &refreshToken, String &athleteName);

StravaError fetchLastActivity(const String &accessToken, StravaActivity &activity);

// Loads one activity by its Strava ID (the number in strava.com/activities/<id>).
// Unlike fetchLastActivity(), this also fills description, calories and device_name.
StravaError fetchActivityById(const String &accessToken, uint64_t id, StravaActivity &activity);

// Loads the heart rate samples of the activity (one extra request) into activity.heart_rate.
// Leaves count at 0 if the activity has no heart rate data.
StravaError fetchHeartRate(const String &accessToken, StravaActivity &activity);

#endif  // STRAVA_API_H
