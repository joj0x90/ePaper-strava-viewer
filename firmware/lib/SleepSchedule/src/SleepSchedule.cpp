#include "SleepSchedule.h"

#include <esp_sleep.h>
#include <esp_sntp.h>
#include <time.h>

// Europe/Berlin incl. daylight saving time, for printLocalTime(). Full hours are the same in
// UTC and in any time zone with a whole-hour offset, so the schedule itself doesn't depend on it.
static const char *TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

bool syncClock(uint32_t timeoutMs) {
    configTzTime(TIME_ZONE, "pool.ntp.org", "time.google.com");
    // Wait for an actual NTP answer: after deep sleep the clock is already set (from the RTC),
    // so checking time(nullptr) alone would succeed without syncing
    uint32_t start = millis();
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        if (millis() - start >= timeoutMs) return false;
        delay(100);
    }
    return true;
}

bool clockIsSet() {
    return time(nullptr) > 1700000000;  // any time after 2023 means it was set at some point
}

uint32_t secondsUntilNextFullHour() {
    return 3600 - time(nullptr) % 3600;
}

void printLocalTime(const char *prefix) {
    setenv("TZ", TIME_ZONE, 1);  // also needed after deep sleep, before syncClock()
    tzset();
    time_t now = time(nullptr);
    char text[24];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", localtime(&now));
    Serial.printf("%s %s\n", prefix, clockIsSet() ? text : "(clock not set)");
}

bool wokeFromDeepSleep() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

void deepSleepFor(uint32_t seconds) {
    Serial.printf("Deep sleep for %u s\n", (unsigned)seconds);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}
