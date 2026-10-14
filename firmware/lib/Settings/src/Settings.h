#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// WiFi and Strava credentials, kept in NVS: the release builds from CI are public and contain
// none. They're entered on the setup page, or stored by a local build with include/secrets.h.
struct Credentials {
    String wifiSsid, wifiPassword;
    String stravaClientId, stravaClientSecret, stravaRefreshToken;
};

// All strings stay empty if nothing was stored yet.
Credentials loadCredentials();
void saveCredentials(const Credentials &credentials);

// Stores the credentials (from secrets.h) only if none are stored yet, so they don't overwrite
// what was entered on the setup page, and not after eraseAllSettings() until a new firmware is
// flashed, so a reset really leaves the device without credentials.
void provisionCredentials(const Credentials &credentials);

// Deletes every NVS entry: the credentials, the access point password and what the display
// shows. Only keeps a note that blocks provisionCredentials() for this firmware. Stop WiFi
// first, it keeps its own data in NVS as well.
void eraseAllSettings();

// Hash of the running firmware, changes with every build that changes the code.
String firmwareHash();

// The device's own WiFi for the setup page, e.g. "StravaViewer-3F2A" (from the MAC address).
String accessPointSsid();
// Random, created on first use and kept until eraseAllSettings(), so a QR code with it stays
// valid on the display.
String accessPointPassword();

#endif  // SETTINGS_H
