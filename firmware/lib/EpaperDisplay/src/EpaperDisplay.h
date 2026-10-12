#ifndef EPAPER_DISPLAY_H
#define EPAPER_DISPLAY_H

#include <Arduino.h>

#include "StravaApi.h"

// Sets up SPI and the 7.5" e-paper panel. Call once before drawing.
void initDisplay();

// footerNotice is shown next to the firmware version, e.g. a failed update.
void drawActivity(StravaActivity activity, const char *footerNotice = "");
void drawError(const char *message);
// Shown while a new firmware is downloaded, which delays the activity.
void drawUpdate(const char *fromVersion, const char *toVersion);

// Test screen with every sport type, its icon and its name.
void drawIconOverviewScreen();

// Puts the panel into deep sleep; call initDisplay() again before the next draw.
void hibernateDisplay();

#endif  // EPAPER_DISPLAY_H
