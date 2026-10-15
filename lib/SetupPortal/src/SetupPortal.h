#ifndef SETUP_PORTAL_H
#define SETUP_PORTAL_H

#include <Arduino.h>

// The setup page in the device's own access point; most phones open it by themselves after
// connecting (captive portal)
#define SETUP_AP_URL "http://192.168.4.1/"
// The setup page via mDNS, when it runs in the normal WiFi
#define SETUP_MDNS_URL "http://strava-viewer.local/"

enum class SetupResult : uint8_t {
    Saved,     // new credentials are stored
    Reset,     // all settings were deleted
    TimedOut,  // nothing changed
    Update,    // install availableUpdate() (OtaUpdate.h)
};

// Shown on the setup page
struct DeviceStatus {
    float batteryVolts;
    int8_t batteryPercent;  // -1: no battery
};

// Starts the setup page for durationMs: in the current WiFi connection, or with accessPoint in
// the device's own WiFi (accessPointSsid() from Settings.h). Returns the page's URL.
String startSetupPortal(bool accessPoint, const DeviceStatus &status, uint32_t durationMs);

// Serves the setup page until the settings are saved or reset, an update is requested, or the
// time is up. Stops the web server and WiFi afterwards; restart then, so the device starts with
// the new settings, or connect again and install the update.
SetupResult runSetupPortal();

#endif  // SETUP_PORTAL_H
