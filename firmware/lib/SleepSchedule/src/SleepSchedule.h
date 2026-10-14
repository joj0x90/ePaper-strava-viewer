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

// True if this start is a wake-up by the timer of deepSleepFor(), false after power-on, reset or
// a button press.
bool wokeFromDeepSleep();

// True if this start is a wake-up by the button (GPIO33 to GND, see the README).
bool wokeByButton();

// Turns everything off except the RTC timer and the button. After waking up by either, the
// ESP32 restarts and runs setup() again; this function never returns.
[[noreturn]] void deepSleepFor(uint32_t seconds);

// Like deepSleepFor(), but only the button wakes the ESP32 up.
[[noreturn]] void deepSleepUntilButton();

#endif  // SLEEP_SCHEDULE_H
