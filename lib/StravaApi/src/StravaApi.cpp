#include "StravaApi.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "SampleAverager.h"

static WiFiClientSecure client;

static void initClient() {
    client.setInsecure();  // skip TLS certificate validation
    // Read timeout for parsing straight from the stream (getJson). WiFiClientSecure::setTimeout()
    // only sets the connect timeout, so set the Stream one (default 1 s) directly; the server
    // can pause longer than that while sending a detailed activity.
    client.Stream::setTimeout(20000);
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
        case StravaErrorCode::ActivityNotFound:      return "Activity not found";
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

StravaError exchangeAuthorizationCode(const char *clientId, const char *clientSecret,
                                      const char *code, String &refreshToken, String &athleteName) {
    initClient();
    HTTPClient http;
    http.begin(client, "https://www.strava.com/oauth/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = String("client_id=") + clientId + "&client_secret=" + clientSecret +
                  "&code=" + code + "&grant_type=authorization_code";
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
    refreshToken = doc["refresh_token"] | "";
    if (refreshToken.isEmpty()) {
        return makeError(StravaErrorCode::TokenMissing, status);
    }
    athleteName =
        String(doc["athlete"]["firstname"] | "") + " " + String(doc["athlete"]["lastname"] | "");
    athleteName.trim();
    return StravaError();
}

// Only keep the fields we need, to save memory
static void addActivityFilter(JsonObject filter) {
    filter["id"] = true;
    filter["sport_type"] = true;
    filter["name"] = true;
    filter["description"] = true;
    filter["start_date_local"] = true;
    filter["distance"] = true;
    filter["map"]["summary_polyline"] = true;
    filter["moving_time"] = true;
    filter["total_elevation_gain"] = true;
    filter["achievement_count"] = true;
    filter["kudos_count"] = true;
    filter["comment_count"] = true;
    filter["device_name"] = true;
    filter["average_speed"] = true;
    filter["max_speed"] = true;
    filter["calories"] = true;
    filter["has_heartrate"] = true;
    filter["average_heartrate"] = true;
    filter["max_heartrate"] = true;
}

static void parseActivity(JsonObjectConst a, StravaActivity &activity) {
    activity.id = a["id"].as<uint64_t>();
    activity.type = a["sport_type"].as<String>();
    activity.name = a["name"].as<String>();
    activity.description = a["description"] | "";
    activity.date = formatDate(a["start_date_local"].as<String>());
    activity.distance_km = a["distance"].as<float>() / 1000.0f;
    activity.showMap = strlen(a["map"]["summary_polyline"] | "") > 0;
    activity.map = a["map"]["summary_polyline"].as<String>();
    activity.moving_time_s = a["moving_time"].as<uint32_t>();
    activity.total_elev_m = a["total_elevation_gain"].as<float>();
    activity.achievement_count = a["achievement_count"].as<uint8_t>();
    activity.kudos_count = a["kudos_count"].as<uint8_t>();
    activity.comment_count = a["comment_count"].as<uint8_t>();
    activity.device_name = a["device_name"] | "";
    activity.average_speed = a["average_speed"].as<float>();
    activity.max_speed = a["max_speed"].as<float>();
    activity.burned_calories = a["calories"].as<float>();
    activity.has_heartrate = a["has_heartrate"] | false;
    activity.average_heartrate = a["average_heartrate"] | 0.0f;
    activity.max_heartrate = a["max_heartrate"] | 0.0f;
    activity.heart_rate.count = 0;
}

// Passes reads through to another stream and records what was read, so a parse
// error can report how far the response got.
class ReadRecorder : public Stream {
public:
    explicit ReadRecorder(Stream &source, unsigned long timeoutMs) : source_(source) {
        setTimeout(timeoutMs);  // ArduinoJson's readBytes() uses this stream's timeout
    }

    int available() override { return source_.available(); }
    int peek() override { return source_.peek(); }
    int read() override {
        int c = source_.read();
        if (c >= 0) {
            bytesRead_++;
            tail_[tailPos_++ % sizeof(tail_)] = (char)c;
        }
        return c;
    }
    size_t write(uint8_t) override { return 0; }

    size_t bytesRead() const { return bytesRead_; }
    // Last bytes read, oldest first
    String tail() const {
        String out;
        size_t n = tailPos_ < sizeof(tail_) ? tailPos_ : sizeof(tail_);
        for (size_t i = tailPos_ - n; i < tailPos_; i++) out += tail_[i % sizeof(tail_)];
        return out;
    }

private:
    Stream &source_;
    size_t bytesRead_ = 0;
    char tail_[48];
    size_t tailPos_ = 0;
};

// GETs an authorized Strava API URL. If it answers with 200, the response can be read from
// http.getStream(); otherwise http is already ended.
static StravaError startGet(HTTPClient &http, const String &url, const String &accessToken) {
    initClient();
    http.useHTTP10(true);  // no chunked encoding, so the response stream is plain JSON
    http.begin(client, url);
    http.addHeader("Authorization", "Bearer " + accessToken);
    const char *headerKeys[] = {"Transfer-Encoding", "Content-Encoding"};
    http.collectHeaders(headerKeys, 2);

    int status = http.GET();
    if (status != HTTP_CODE_OK) {
        String response = http.getString();
        http.end();
        StravaErrorCode code = status == HTTP_CODE_NOT_FOUND
                                   ? StravaErrorCode::ActivityNotFound
                                   : StravaErrorCode::ActivityRequestFailed;
        return makeError(code, status, response);
    }
    return StravaError();
}

// GETs an authorized Strava API URL and parses the filtered JSON response into doc.
static StravaError getJson(const String &url, const String &accessToken, const JsonDocument &filter,
                           JsonDocument &doc) {
    HTTPClient http;
    StravaError error = startGet(http, url, accessToken);
    if (!error.ok()) return error;

    // Parse straight from the connection: detailed activities can be larger than the free RAM
    unsigned long start = millis();
    ReadRecorder stream(http.getStream(), 20000);
    DeserializationError err = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
    if (err) {
        char info[160];
        snprintf(info, sizeof(info),
                 "%s after %u of %d bytes, %lu ms, connected=%d, "
                 "Transfer-Encoding='%s', Content-Encoding='%s', last bytes:\n",
                 err.c_str(), (unsigned)stream.bytesRead(), http.getSize(), millis() - start,
                 http.connected(), http.header("Transfer-Encoding").c_str(),
                 http.header("Content-Encoding").c_str());
        String detail = String(info) + stream.tail();
        http.end();
        return makeError(StravaErrorCode::ActivityInvalidJson, HTTP_CODE_OK, detail);
    }
    http.end();
    return StravaError();
}

StravaError fetchLastActivity(const String &accessToken, StravaActivity &activity) {
    JsonDocument filter;
    addActivityFilter(filter[0].to<JsonObject>());

    JsonDocument doc;
    StravaError error = getJson("https://www.strava.com/api/v3/athlete/activities?per_page=1",
                                accessToken, filter, doc);
    if (!error.ok()) return error;
    if (doc.as<JsonArrayConst>().size() == 0) {
        return makeError(StravaErrorCode::NoActivities, HTTP_CODE_OK);
    }

    parseActivity(doc[0], activity);
    return StravaError();
}

StravaError fetchActivityById(const String &accessToken, uint64_t id, StravaActivity &activity) {
    JsonDocument filter;
    addActivityFilter(filter.to<JsonObject>());

    char url[96];
    snprintf(url, sizeof(url),
             "https://www.strava.com/api/v3/activities/%llu?include_all_efforts=true",
             (unsigned long long)id);

    JsonDocument doc;
    StravaError error = getJson(url, accessToken, filter, doc);
    if (!error.ok()) return error;

    parseActivity(doc.as<JsonObjectConst>(), activity);
    return StravaError();
}

// Reads one character, -1 at the end of the response (or after the stream's timeout)
static int readChar(Stream &stream) {
    char c;
    return stream.readBytes(&c, 1) == 1 ? (uint8_t)c : -1;
}

// Reads the response of the streams endpoint up to the end of the "heartrate" stream's data
// array, and averages its numbers. Parsed by hand, as an activity can have tens of thousands of
// samples, which don't fit into the RAM as a JsonDocument. The response looks like
// {"heartrate":{"data":[92,95,...],"series_type":"distance",...},"distance":{...}}; its strings
// contain no brackets. Returns false if it ended before the heart rate data did.
static bool readHeartRateSamples(Stream &stream, SampleAverager &averager) {
    static const char KEY[] = "\"heartrate\"";
    size_t matched = 0;  // characters of KEY matched so far
    int depth = 0;       // nesting of {}
    int c;
    // find the key at the top level, then the start of its data array
    while (true) {
        c = readChar(stream);
        if (c < 0) return false;
        if (c == '{') depth++;
        if (c == '}' && --depth == 0) return true;  // end of the response: no heart rate stream
        matched = c == KEY[matched] ? matched + 1 : (c == KEY[0] ? 1 : 0);
        if (matched == sizeof(KEY) - 1 && depth == 1) break;
    }
    do {
        c = readChar(stream);
        if (c < 0 || c == '}') return false;  // a heart rate object without data
    } while (c != '[');

    uint32_t value = 0;
    bool inNumber = false, inFraction = false;
    while ((c = readChar(stream)) >= 0) {
        if (isdigit(c)) {
            if (!inFraction) value = min<uint32_t>(value * 10 + (c - '0'), UINT16_MAX);
            inNumber = true;
        } else if (c == '.') {
            inFraction = true;  // integers are expected, but cut off decimals just in case
        } else {
            if (inNumber) averager.add(value);
            value = 0;
            inNumber = inFraction = false;
            if (c == ']') return true;
        }
    }
    return false;
}

StravaError fetchHeartRate(const String &accessToken, StravaActivity &activity) {
    activity.heart_rate.count = 0;
    char url[112];
    snprintf(url, sizeof(url),
             "https://www.strava.com/api/v3/activities/%llu/streams?keys=heartrate&key_by_type=true",
             (unsigned long long)activity.id);

    HTTPClient http;
    StravaError error = startGet(http, url, accessToken);
    if (!error.ok()) return error;

    static SampleAverager averager;  // 8 KB, too large for the stack
    averager.reset();
    ReadRecorder stream(http.getStream(), 20000);
    const bool complete = readHeartRateSamples(stream, averager);
    http.end();
    if (!complete) {
        char info[64];
        snprintf(info, sizeof(info), "heart rate stream ended after %u bytes, last bytes:\n",
                 (unsigned)stream.bytesRead());
        return makeError(StravaErrorCode::ActivityInvalidJson, HTTP_CODE_OK,
                         String(info) + stream.tail());
    }

    activity.heart_rate.count = averager.averages(activity.heart_rate.bpm, HEART_RATE_BARS);
    Serial.printf("Heart rate: %lu samples in %u bars\n", (unsigned long)averager.sampleCount(),
                  activity.heart_rate.count);
    return StravaError();
}
