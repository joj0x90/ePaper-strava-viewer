#ifndef SLEEP_SCHEDULE_H
#define SLEEP_SCHEDULE_H

#include <Arduino.h>

// Sets the system clock via NTP (needs WiFi). Returns false if no answer within timeoutMs.
bool syncClock(uint32_t timeoutMs = 10000);

// True once the clock has been set. The time survives deep sleep (kept by the RTC timer),
// but drifts with it, so call syncClock() whenever WiFi is available.
bool clockIsSet();

// Seconds until the next full hour by the system clock; only meaningful if clockIsSet().
uint32_t secondsUntilNextFullHour();

// Prints the current local time, e.g. "Woke up at 2026-10-11 14:00:04".
void printLocalTime(const char *prefix);

// True if this start is a wake-up from deepSleepFor(), false after power-on or reset.
bool wokeFromDeepSleep();

// Turns everything off except the RTC timer. After waking up, the ESP32 restarts and
// runs setup() again; this function never returns.
[[noreturn]] void deepSleepFor(uint32_t seconds);

#endif  // SLEEP_SCHEDULE_H
