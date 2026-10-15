#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include <Arduino.h>

// Set by tools/version.py: the git tag for release builds, e.g. "v0.1.0", otherwise
// the `git describe` output, e.g. "v0.1.0-3-gabc1234-dirty".
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// Called with the new version right before its download starts, e.g. to tell the user.
typedef void (*UpdateFoundCallback)(const char *newVersion);

// Checks the latest GitHub release and, if its tag differs from FIRMWARE_VERSION, installs its
// firmware.bin and restarts. Only release builds update themselves, so a firmware flashed via
// USB during development isn't replaced. Needs WiFi; returns if there is nothing to do ("") or
// with a short error for the display, e.g. "unknown certificate".
String updateFirmwareIfAvailable(UpdateFoundCallback onUpdateFound);

#endif  // OTA_UPDATE_H
