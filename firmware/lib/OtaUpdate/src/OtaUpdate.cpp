#include "OtaUpdate.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <string.h>

#include "GithubRootCas.h"

#define GITHUB_REPO "joj0x90/ePaper-strava-viewer"
#define FIRMWARE_ASSET "firmware.bin"

// Release builds are exactly a tag like "v1.2.3"; `git describe` adds "-<commits>-g<hash>"
// and "-dirty" for everything else.
static bool isReleaseBuild() {
    return FIRMWARE_VERSION[0] == 'v' && strchr(FIRMWARE_VERSION, '-') == nullptr;
}

// Tag of the latest release, or "" if it couldn't be fetched.
static String fetchLatestTag(WiFiClientSecure &client) {
    HTTPClient http;
    http.begin(client, "https://api.github.com/repos/" GITHUB_REPO "/releases/latest");
    http.addHeader("Accept", "application/vnd.github+json");
    http.setUserAgent("ePaper-strava-viewer/" FIRMWARE_VERSION);  // required by the GitHub API
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        Serial.printf("OTA: release request failed (HTTP %d)\n", status);
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
        Serial.printf("OTA: invalid release response: %s\n", err.c_str());
        return "";
    }
    return doc["tag_name"] | "";
}

void updateFirmwareIfAvailable() {
    if (!isReleaseBuild()) {
        Serial.printf("OTA: %s is no release build, not updating\n", FIRMWARE_VERSION);
        return;
    }

    WiFiClientSecure client;
    client.setCACert(GITHUB_ROOT_CAS);

    const String tag = fetchLatestTag(client);
    if (tag.isEmpty() || tag == FIRMWARE_VERSION) return;

    // redirects to the actual file on release-assets.githubusercontent.com
    const String url =
        "https://github.com/" GITHUB_REPO "/releases/download/" + tag + "/" FIRMWARE_ASSET;
    Serial.printf("OTA: updating %s -> %s\n", FIRMWARE_VERSION, tag.c_str());
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);
    if (httpUpdate.update(client, url) == HTTP_UPDATE_FAILED) {
        Serial.printf("OTA: update failed (%d): %s\n", httpUpdate.getLastError(),
                      httpUpdate.getLastErrorString().c_str());
    }
}
