#ifndef EPAPER_DISPLAY_H
#define EPAPER_DISPLAY_H

#include <Arduino.h>

#include "StravaApi.h"

// Sets up SPI and the 7.5" e-paper panel. Call once before drawing.
void initDisplay();

void drawActivity(StravaActivity activity);
void drawError(const char *message);

// Puts the panel into deep sleep; call initDisplay() again before the next draw.
void hibernateDisplay();

#endif  // EPAPER_DISPLAY_H
