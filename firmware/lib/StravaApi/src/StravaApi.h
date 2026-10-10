#ifndef STRAVA_API_H
#define STRAVA_API_H

#include <Arduino.h>

struct Activity {
    String type;   // e.g. "Trail Run"
    String title;  // e.g. "Morning Run"
    String date;   // e.g. "09.10.2026 07:12"
    float distanceKm;
    bool showMap;  // true for most activities, like runs and rides, false for e.g. indoor swims.
    uint32_t mocing_time_s;  // in seconds
};

enum class StravaErrorCode : uint8_t {
    None = 0,
    TokenRequestFailed,  // token endpoint did not answer with HTTP 200
    TokenInvalidJson,
    TokenMissing,           // response had no access_token
    ActivityRequestFailed,  // activities endpoint did not answer with HTTP 200
    ActivityInvalidJson,
    NoActivities,
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

StravaError fetchLastActivity(const String &accessToken, Activity &activity);

#endif  // STRAVA_API_H
