#ifndef HEART_RATE_CHART_H
#define HEART_RATE_CHART_H

#include <Adafruit_GFX.h>

// Bar chart of the heart rate over the activity, filling the box (x, y, w, h): a heart icon
// with the average and maximum heart rate on top, below it one bar per value in bpm, from left
// (start) to right (end), with a bpm scale on the left. Bars of 0 (no data) are left out.
// font must contain Latin-1 (for "Ø"); background is used for the icon's cut-outs.
void drawHeartRateChart(Adafruit_GFX &gfx, const uint8_t *bpm, uint8_t bars, uint8_t averageBpm,
                        uint8_t maxBpm, const GFXfont *font, int16_t x, int16_t y, int16_t w,
                        int16_t h, uint16_t color, uint16_t background);

#endif  // HEART_RATE_CHART_H
