#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <WiFi.h>

enum class WifiErrorCode : uint8_t {
    None = 0,
    NoSsidAvailable,  // network not found
    ConnectFailed,    // usually a wrong password
    Timeout,          // no connection within the timeout
};

struct WifiError {
    WifiErrorCode code = WifiErrorCode::None;
    wl_status_t status = WL_IDLE_STATUS;  // raw status reported by the WiFi driver

    bool ok() const { return code == WifiErrorCode::None; }
    const char *message() const;
};

// Connects in station mode, blocking for at most timeoutMs.
WifiError connectWifi(const char *ssid, const char *password, uint32_t timeoutMs = 20000);
void disconnectWifi();

#endif  // WIFI_CONNECT_H
