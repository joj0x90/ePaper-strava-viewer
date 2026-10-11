#include "StravaApi.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure client;

static void initClient() {
    client.setInsecure();  // skip TLS certificate validation
}

const char *StravaError::message() const {
    switch (code) {
        case StravaErrorCode::None:                  return "OK";
        case StravaErrorCode::TokenRequestFailed:    return "Strava login failed";
        case StravaErrorCode::TokenInvalidJson:      return "Strava login: invalid response";
        case StravaErrorCode::TokenMissing:          return "Strava login: no access token";
        case StravaErrorCode::ActivityRequestFailed: return "Could not load activity";
        case StravaErrorCode::ActivityInvalidJson:   return "Activity: invalid response";
        case StravaErrorCode::NoActivities:          return "No activities found";
    }
    return "Unknown Strava error";
}

static StravaError makeError(StravaErrorCode code, int httpStatus = 0, const String &detail = "") {
    StravaError error;
    error.code = code;
    error.httpStatus = httpStatus;
    error.detail = detail;
    return error;
}

// "yyyy-mm-ddTHH:MM:ssZ" -> "dd.mm.yyyy HH:mm"
static String formatDate(const String &iso) {
    if (iso.length() < 16) return iso;
    return iso.substring(8, 10) + "." + iso.substring(5, 7) + "." + iso.substring(0, 4) + " " +
           iso.substring(11, 16);
}

StravaError fetchAccessToken(const char *clientId, const char *clientSecret,
                             const char *refreshToken, String &accessToken) {
    initClient();
    HTTPClient http;
    http.begin(client, "https://www.strava.com/oauth/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = String("client_id=") + clientId + "&client_secret=" + clientSecret +
                  "&refresh_token=" + refreshToken + "&grant_type=refresh_token";
    int status = http.POST(body);
    String response = http.getString();
    http.end();
    if (status != HTTP_CODE_OK) {
        return makeError(StravaErrorCode::TokenRequestFailed, status, response);
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, response);
    if (err) {
        return makeError(StravaErrorCode::TokenInvalidJson, status, err.c_str());
    }
    accessToken = doc["access_token"] | "";
    if (accessToken.isEmpty()) {
        return makeError(StravaErrorCode::TokenMissing, status);
    }
    return StravaError();
}

StravaError fetchLastActivity(const String &accessToken, StravaActivity &activity) {
    initClient();
    HTTPClient http;
    http.begin(client, "https://www.strava.com/api/v3/athlete/activities?per_page=1");
    http.addHeader("Authorization", "Bearer " + accessToken);

    int status = http.GET();
    String response = http.getString();
    http.end();
    if (status != HTTP_CODE_OK) {
        return makeError(StravaErrorCode::ActivityRequestFailed, status, response);
    }

    // Only keep the fields we need to save memory
    JsonDocument filter;
    filter[0]["sport_type"] = true;
    filter[0]["name"] = true;
    filter[0]["description"] = true;
    filter[0]["start_date_local"] = true;
    filter[0]["distance"] = true;
    filter[0]["map"] = true;
    filter[0]["moving_time"] = true;
    filter[0]["total_elevation_gain"] = true;
    filter[0]["achievement_count"] = true;
    filter[0]["kudos_count"] = true;
    filter[0]["comment_count"] = true;
    filter[0]["device_name"] = true;
    filter[0]["average_speed"] = true;
    filter[0]["max_speed"] = true;
    filter[0]["calories"] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, response, DeserializationOption::Filter(filter));
    if (err) {
        return makeError(StravaErrorCode::ActivityInvalidJson, status, err.c_str());
    }
    if (doc.as<JsonArray>().size() == 0) {
        return makeError(StravaErrorCode::NoActivities, status);
    }

    JsonObject a = doc[0];
    activity.type = a["sport_type"].as<String>();
    activity.name = a["name"].as<String>();
    activity.description = a["description"].as<String>();
    activity.date = formatDate(a["start_date_local"].as<String>());
    activity.distance_km = a["distance"].as<float>() / 1000.0f;
    activity.showMap = a["map"]["summary_polyline"] | "" != "";
    activity.moving_time_s = a["moving_time"].as<uint32_t>();
    activity.total_elev_m = a["total_elevation_gain"].as<float>();
    activity.achievement_count = a["achievement_count"].as<uint8_t>();
    activity.kudos_count = a["kudos_count"].as<uint8_t>();
    activity.comment_count = a["comment_count"].as<uint8_t>();
    activity.device_name = a["device_name"].as<String>();
    activity.average_speed = a["average_speed"].as<float>();
    activity.max_speed = a["max_speed"].as<float>();
    activity.burned_calories = a["calories"].as<float>();
    return StravaError();
}
