#ifndef ICON_OVERVIEW_H
#define ICON_OVERVIEW_H

#include <Adafruit_GFX.h>

// Draws a grid over the whole canvas: every mapped Strava sport type with its
// icon and name, then the fallback icon ("(other)") and the error icon ("(error)").
// Used by the icon-test firmware and the PC preview in tools/icon_overview.
void drawIconOverview(Adafruit_GFX &gfx, uint16_t color, uint16_t background);

#endif  // ICON_OVERVIEW_H
