#include "WifiConnect.h"

const char *WifiError::message() const {
  switch (code) {
    case WifiErrorCode::None:            return "OK";
    case WifiErrorCode::NoSsidAvailable: return "WiFi network not found";
    case WifiErrorCode::ConnectFailed:   return "WiFi connection failed";
    case WifiErrorCode::Timeout:         return "WiFi connection timed out";
  }
  return "Unknown WiFi error";
}

WifiError connectWifi(const char *ssid, const char *password, uint32_t timeoutMs) {
  Serial.printf("Connecting to WiFi '%s'", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  WifiError result;
  uint32_t start = millis();
  while ((result.status = WiFi.status()) != WL_CONNECTED) {
    if (result.status == WL_NO_SSID_AVAIL) {
      result.code = WifiErrorCode::NoSsidAvailable;
      break;
    }
    if (result.status == WL_CONNECT_FAILED) {
      result.code = WifiErrorCode::ConnectFailed;
      break;
    }
    if (millis() - start >= timeoutMs) {
      result.code = WifiErrorCode::Timeout;
      break;
    }
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (result.ok()) {
    Serial.printf("Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  return result;
}

void disconnectWifi() {
  WiFi.disconnect(true);
}
