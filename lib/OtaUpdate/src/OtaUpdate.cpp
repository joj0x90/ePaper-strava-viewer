#include "OtaUpdate.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <mbedtls/x509.h>
#include <string.h>

#include "GithubRootCas.h"

#define GITHUB_REPO "joj0x90/ePaper-strava-viewer"
#define FIRMWARE_ASSET "firmware.bin"

// The release found by the last check, so the setup page can offer it without internet
static const char *PREFS_NAMESPACE = "ota";
static const char *PREFS_AVAILABLE = "available";

// Release builds are exactly a tag like "v1.2.3"; `git describe` adds "-<commits>-g<hash>"
// and "-dirty" for everything else.
static bool isReleaseBuild() {
    return FIRMWARE_VERSION[0] == 'v' && strchr(FIRMWARE_VERSION, '-') == nullptr;
}

// Short description of a failed request: the TLS error if the connection failed there,
// otherwise the fallback.
static String describeError(WiFiClientSecure &client, const String &fallback) {
    char text[64];
    const int tlsError = client.lastError(text, sizeof(text));
    if (tlsError == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) return "unknown certificate";
    if (tlsError != 0) return text;
    return fallback;
}

// Tag of the latest release; on failure "" with the reason in error.
static String fetchLatestTag(WiFiClientSecure &client, String &error) {
    HTTPClient http;
    http.begin(client, "https://api.github.com/repos/" GITHUB_REPO "/releases/latest");
    http.addHeader("Accept", "application/vnd.github+json");
    http.setUserAgent("ePaper-strava-viewer/" FIRMWARE_VERSION);  // required by the GitHub API
    const int status = http.GET();
    if (status == HTTP_CODE_NOT_FOUND) {  // no release published yet
        http.end();
        return "";
    }
    if (status != HTTP_CODE_OK) {
        error = describeError(
            client, status < 0 ? HTTPClient::errorToString(status) : "HTTP " + String(status));
        http.end();
        return "";
    }

    JsonDocument filter;
    filter["tag_name"] = true;
    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        error = String("invalid response: ") + err.c_str();
        return "";
    }
    return doc["tag_name"] | "";
}

String checkForUpdate() {
    if (!isReleaseBuild()) {
        Serial.printf("OTA: %s is no release build, not checking for updates\n", FIRMWARE_VERSION);
        return "";
    }
    WiFiClientSecure client;
    client.setCACert(GITHUB_ROOT_CAS);
    String error;
    const String tag = fetchLatestTag(client, error);
    if (!error.isEmpty()) {
        Serial.printf("OTA: update check failed: %s\n", error.c_str());
        return error;
    }
    const String available = tag == FIRMWARE_VERSION ? "" : tag;
    if (!available.isEmpty()) {
        Serial.printf("OTA: %s is available (running %s)\n", tag.c_str(), FIRMWARE_VERSION);
    }
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    // written only when it changes, to spare the flash an hourly write
    if (prefs.getString(PREFS_AVAILABLE) != available) prefs.putString(PREFS_AVAILABLE, available);
    prefs.end();
    return "";
}

String availableUpdate() {
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return "";  // never checked
    const String tag = prefs.getString(PREFS_AVAILABLE);
    prefs.end();
    // the stored tag is installed already if the update restarted before the next check
    return tag == FIRMWARE_VERSION ? "" : tag;
}

String installUpdate(const String &tag, UpdateFoundCallback onUpdateFound) {
    Serial.printf("OTA: updating %s -> %s\n", FIRMWARE_VERSION, tag.c_str());
    onUpdateFound(tag.c_str());
    WiFiClientSecure client;
    client.setCACert(GITHUB_ROOT_CAS);
    // redirects to the actual file on release-assets.githubusercontent.com
    const String url =
        "https://github.com/" GITHUB_REPO "/releases/download/" + tag + "/" FIRMWARE_ASSET;
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);
    httpUpdate.update(client, url);  // only returns if the update failed
    const String error = describeError(client, httpUpdate.getLastErrorString());
    Serial.printf("OTA: update failed (%d): %s\n", httpUpdate.getLastError(), error.c_str());
    return error;
}
