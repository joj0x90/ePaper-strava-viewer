#include "Settings.h"

#include <Preferences.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <nvs_flash.h>

static const char *PREFS_CREDENTIALS = "credentials";
static const char *PREFS_SETUP = "setup";
static const char *PREFS_AP_PASSWORD = "apPassword";
// Firmware hash of the build that was reset: its secrets.h isn't stored again
static const char *PREFS_RESET_FIRMWARE = "resetFw";

static const uint8_t AP_PASSWORD_LENGTH = 10;
// Without look-alikes (0/O, 1/l/I), as it's typed from the display if the QR code isn't used
static const char AP_PASSWORD_CHARS[] = "abcdefghjkmnpqrstuvwxyz23456789";

Credentials loadCredentials() {
    Credentials c;
    Preferences prefs;
    if (!prefs.begin(PREFS_CREDENTIALS, true)) return c;  // never stored: all strings stay empty
    c.wifiSsid = prefs.getString("wifiSsid");
    c.wifiPassword = prefs.getString("wifiPassword");
    c.stravaClientId = prefs.getString("clientId");
    c.stravaClientSecret = prefs.getString("clientSecret");
    c.stravaRefreshToken = prefs.getString("refreshToken");
    prefs.end();
    return c;
}

void saveCredentials(const Credentials &c) {
    Preferences prefs;
    prefs.begin(PREFS_CREDENTIALS, false);
    prefs.putString("wifiSsid", c.wifiSsid);
    prefs.putString("wifiPassword", c.wifiPassword);
    prefs.putString("clientId", c.stravaClientId);
    prefs.putString("clientSecret", c.stravaClientSecret);
    prefs.putString("refreshToken", c.stravaRefreshToken);
    prefs.end();
}

String firmwareHash() {
    char hash[17];  // the first 16 hex digits are enough to tell builds apart
    esp_ota_get_app_elf_sha256(hash, sizeof(hash));
    return hash;
}

void provisionCredentials(const Credentials &credentials) {
    if (!loadCredentials().wifiSsid.isEmpty()) return;

    Preferences prefs;
    if (prefs.begin(PREFS_SETUP, true)) {
        const bool reset = prefs.getString(PREFS_RESET_FIRMWARE) == firmwareHash();
        prefs.end();
        if (reset) {
            Serial.println("Not storing secrets.h: the device was reset, flash a new build for it");
            return;
        }
    }
    Serial.println("Storing the credentials from secrets.h");
    saveCredentials(credentials);
}

void eraseAllSettings() {
    esp_err_t erased = nvs_flash_erase();
    esp_err_t initialized = nvs_flash_init();
    Serial.printf("NVS erased: %s, initialized: %s\n", esp_err_to_name(erased),
                  esp_err_to_name(initialized));

    Preferences prefs;
    prefs.begin(PREFS_SETUP, false);
    prefs.putString(PREFS_RESET_FIRMWARE, firmwareHash());
    prefs.end();
}

String accessPointSsid() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    char ssid[20];
    snprintf(ssid, sizeof(ssid), "StravaViewer-%02X%02X", mac[4], mac[5]);
    return ssid;
}

String accessPointPassword() {
    Preferences prefs;
    prefs.begin(PREFS_SETUP, false);
    String password = prefs.getString(PREFS_AP_PASSWORD);
    if (password.isEmpty()) {
        for (uint8_t i = 0; i < AP_PASSWORD_LENGTH; i++) {
            password += AP_PASSWORD_CHARS[esp_random() % (sizeof(AP_PASSWORD_CHARS) - 1)];
        }
        prefs.putString(PREFS_AP_PASSWORD, password);
    }
    prefs.end();
    return password;
}
