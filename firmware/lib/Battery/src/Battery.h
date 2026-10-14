#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

// Battery voltage in volts, measured through the 1:2 voltage divider on GPIO35 (see the README
// schematic). Read it before WiFi starts: its current peaks pull the battery voltage down.
float readBatteryVoltage();

// Charge in percent (0-100), estimated from readBatteryVoltage(), or -1 if no battery is
// connected (e.g. powered via USB with the battery disconnected for flashing).
int8_t batteryPercent(float volts);

#endif  // BATTERY_H
