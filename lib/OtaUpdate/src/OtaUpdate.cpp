#include "OtaUpdate.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <mbedtls/x509.h>
#include <string.h>

#include "GithubRootCas.h"

#define GITHUB_REPO "joj0x90/ePaper-strava-viewer"
#define FIRMWARE_ASSET "firmware.bin"

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

String updateFirmwareIfAvailable(UpdateFoundCallback onUpdateFound) {
    if (!isReleaseBuild()) {
        Serial.printf("OTA: %s is no release build, not updating\n", FIRMWARE_VERSION);
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
    if (tag.isEmpty() || tag == FIRMWARE_VERSION) return "";

    Serial.printf("OTA: updating %s -> %s\n", FIRMWARE_VERSION, tag.c_str());
    onUpdateFound(tag.c_str());

    // redirects to the actual file on release-assets.githubusercontent.com
    const String url =
        "https://github.com/" GITHUB_REPO "/releases/download/" + tag + "/" FIRMWARE_ASSET;
    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);
    httpUpdate.update(client, url);  // only returns if the update failed

    error = describeError(client, httpUpdate.getLastErrorString());
    Serial.printf("OTA: update failed (%d): %s\n", httpUpdate.getLastError(), error.c_str());
    return error;
}
