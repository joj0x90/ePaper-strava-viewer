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

// Checks the latest GitHub release and remembers (in NVS) whether its tag differs from
// FIRMWARE_VERSION, for availableUpdate(). Only release builds look for updates, so a firmware
// flashed via USB during development isn't offered a replacement. Needs WiFi; returns "" or a
// short error for the display, e.g. "unknown certificate", which keeps the remembered result.
String checkForUpdate();

// The newer release found by the last checkForUpdate(), or "" if there is none.
String availableUpdate();

// Installs the firmware.bin of the release tag and restarts. Needs WiFi; only returns if the
// update failed, with a short error for the display.
String installUpdate(const String &tag, UpdateFoundCallback onUpdateFound);

#endif  // OTA_UPDATE_H
