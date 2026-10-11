#ifndef STRAVA_API_H
#define STRAVA_API_H

#include <Arduino.h>

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

StravaError fetchLastActivity(const String &accessToken, StravaActivity &activity);

// Loads one activity by its Strava ID (the number in strava.com/activities/<id>).
// Unlike fetchLastActivity(), this also fills description, calories and device_name.
StravaError fetchActivityById(const String &accessToken, uint64_t id, StravaActivity &activity);

#endif  // STRAVA_API_H
