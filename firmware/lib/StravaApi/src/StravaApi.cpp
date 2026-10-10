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

// "TrailRun" -> "Trail Run"
static String splitCamelCase(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    if (i > 0 && isUpperCase(s[i])) out += ' ';
    out += s[i];
  }
  return out;
}

// "yyyy-mm-ddTHH:MM:ssZ" -> "dd.mm.yyyy HH:mm"
static String formatDate(const String &iso) {
  if (iso.length() < 16) return iso;
  return iso.substring(8, 10) + "." + iso.substring(5, 7) + "." + iso.substring(0, 4) +
         " " + iso.substring(11, 16);
}

StravaError fetchAccessToken(const char *clientId, const char *clientSecret,
                             const char *refreshToken, String &accessToken) {
  initClient();
  HTTPClient http;
  http.begin(client, "https://www.strava.com/oauth/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = String("client_id=") + clientId +
                "&client_secret=" + clientSecret +
                "&refresh_token=" + refreshToken +
                "&grant_type=refresh_token";
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

StravaError fetchLastActivity(const String &accessToken, Activity &activity) {
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
  filter[0]["start_date_local"] = true;
  filter[0]["distance"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
  if (err) {
    return makeError(StravaErrorCode::ActivityInvalidJson, status, err.c_str());
  }
  if (doc.as<JsonArray>().size() == 0) {
    return makeError(StravaErrorCode::NoActivities, status);
  }

  JsonObject a = doc[0];
  activity.type = splitCamelCase(a["sport_type"].as<String>());
  activity.date = formatDate(a["start_date_local"].as<String>());
  activity.distanceKm = a["distance"].as<float>() / 1000.0f;
  return StravaError();
}
