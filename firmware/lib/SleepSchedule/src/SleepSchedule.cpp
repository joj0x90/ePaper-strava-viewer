#include "SleepSchedule.h"

#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_sntp.h>
#include <time.h>

// Europe/Berlin incl. daylight saving time, for printLocalTime(). Full hours are the same in
// UTC and in any time zone with a whole-hour offset, so the schedule itself doesn't depend on it.
static const char *TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

// Button between this pin and GND. It must be an RTC GPIO to wake the ESP32 up from deep sleep.
static const gpio_num_t BUTTON_PIN = GPIO_NUM_33;

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

bool wokeByButton() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
}

// Wakes up when the button pulls the pin low. The RTC pull-up stays on during deep sleep and
// only draws current while the button is pressed.
static void enableButtonWakeup() {
    rtc_gpio_pullup_en(BUTTON_PIN);
    rtc_gpio_pulldown_dis(BUTTON_PIN);
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);
}

void deepSleepFor(uint32_t seconds) {
    Serial.printf("Deep sleep for %u s\n", (unsigned)seconds);
    Serial.flush();
    enableButtonWakeup();
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}

void deepSleepUntilButton() {
    Serial.println("Deep sleep until the button is pressed");
    Serial.flush();
    enableButtonWakeup();
    esp_deep_sleep_start();
}
