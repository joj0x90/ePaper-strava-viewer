#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "secrets.h"

// CS=15, DC=27, RST=26, BUSY=25
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(15, 27, 26, 25));

struct Activity {
  String type;      // e.g. "Trail Run"
  String date;      // e.g. "09.10.2026 07:12"
  float distanceKm;
};

bool connectWifi() {
  Serial.printf("Connecting to WiFi '%s'", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed");
    return false;
  }
  Serial.printf("Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// Exchanges the long-lived refresh token for a short-lived access token.
bool fetchAccessToken(WiFiClientSecure &client, String &accessToken) {
  HTTPClient http;
  http.begin(client, "https://www.strava.com/oauth/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = String("client_id=") + STRAVA_CLIENT_ID +
                "&client_secret=" + STRAVA_CLIENT_SECRET +
                "&refresh_token=" + STRAVA_REFRESH_TOKEN +
                "&grant_type=refresh_token";
  int status = http.POST(body);
  if (status != HTTP_CODE_OK) {
    Serial.printf("Token request failed: HTTP %d\n%s\n", status, http.getString().c_str());
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err) {
    Serial.printf("Token JSON error: %s\n", err.c_str());
    return false;
  }
  accessToken = doc["access_token"].as<String>();
  return !accessToken.isEmpty();
}

// "TrailRun" -> "Trail Run"
String splitCamelCase(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    if (i > 0 && isUpperCase(s[i])) out += ' ';
    out += s[i];
  }
  return out;
}

// "yyyy-mm-ddTHH:MM:ssZ" -> "dd.mm.yyyy HH:mm"
String formatDate(const String &iso) {
  if (iso.length() < 16) return iso;
  return iso.substring(8, 10) + "." + iso.substring(5, 7) + "." + iso.substring(0, 4) +
         " " + iso.substring(11, 16);
}

bool fetchLastActivity(WiFiClientSecure &client, const String &accessToken, Activity &activity) {
  HTTPClient http;
  http.begin(client, "https://www.strava.com/api/v3/athlete/activities?per_page=1");
  http.addHeader("Authorization", "Bearer " + accessToken);

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("Activity request failed: HTTP %d\n%s\n", status, http.getString().c_str());
    http.end();
    return false;
  }

  // Only keep the fields we need to save memory
  JsonDocument filter;
  filter[0]["sport_type"] = true;
  filter[0]["start_date_local"] = true;
  filter[0]["distance"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getString(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("Activity JSON error: %s\n", err.c_str());
    return false;
  }
  if (doc.as<JsonArray>().size() == 0) {
    Serial.println("No activities found");
    return false;
  }

  JsonObject a = doc[0];
  activity.type = splitCamelCase(a["sport_type"].as<String>());
  activity.date = formatDate(a["start_date_local"].as<String>());
  activity.distanceKm = a["distance"].as<float>() / 1000.0f;
  return true;
}

void drawActivity(const Activity &activity) {
  char distance[16];
  snprintf(distance, sizeof(distance), "%.2f km", activity.distanceKm);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    display.setFont(&FreeSans18pt7b);
    display.setCursor(40, 70);
    display.print("Last activity");
    display.drawLine(40, 90, display.width() - 40, 90, GxEPD_BLACK);

    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(40, 180);
    display.print(activity.type);

    display.setFont(&FreeSans18pt7b);
    display.setCursor(40, 260);
    display.print(activity.date);

    display.setFont(&FreeSansBold24pt7b);
    display.setCursor(40, 380);
    display.print(distance);
  } while (display.nextPage());
}

void drawError(const char *message) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&FreeSans18pt7b);
    display.setCursor(40, 100);
    display.print("Error:");
    display.setCursor(40, 160);
    display.print(message);
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);

  SPI.end();
  SPI.begin(13, 12, 14, 15);  // SCK=13, MISO=12 (unused), MOSI=14, SS=15
  display.init(115200, true, 2, false);
  display.setRotation(0);

  if (!connectWifi()) {
    drawError("WiFi connection failed");
  } else {
    WiFiClientSecure client;
    client.setInsecure();  // skip TLS certificate validation

    String accessToken;
    Activity activity;
    if (!fetchAccessToken(client, accessToken)) {
      drawError("Strava login failed");
    } else if (!fetchLastActivity(client, accessToken, activity)) {
      drawError("Could not load activity");
    } else {
      Serial.printf("%s | %s | %.2f km\n", activity.type.c_str(), activity.date.c_str(), activity.distanceKm);
      drawActivity(activity);
    }
  }

  WiFi.disconnect(true);
  display.hibernate();
}

void loop() {
}
