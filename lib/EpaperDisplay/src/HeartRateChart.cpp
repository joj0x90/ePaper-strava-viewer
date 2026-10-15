#include "HeartRateChart.h"

#include <stdio.h>

#include "ActivityIcons.h"

namespace {

const int16_t ICON_SIZE = 40;
const int16_t ICON_GAP = 8;    // between the icon and the text
const int16_t PLOT_GAP = 20;   // between the heading and the topmost scale line
const int16_t LABEL_GAP = 6;   // between the scale labels and the bars
const uint8_t SCALE_STEP = 20;  // bpm between the scale lines
const uint8_t MIN_RANGE = 40;   // bpm the scale covers at least, so flat lines don't look wild
const int16_t DOT_SPACING = 4;  // pixels from dot to dot of the scale lines

// Width of the text from the cursor to its right end
int16_t textWidth(Adafruit_GFX &gfx, const char *text) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return x1 + w;
}

// Height of the digits above the baseline
int16_t digitHeight(Adafruit_GFX &gfx) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx.getTextBounds("0", 0, 0, &x1, &y1, &w, &h);
    return -y1;
}

}  // namespace

void drawHeartRateChart(Adafruit_GFX &gfx, const uint8_t *bpm, uint8_t bars, uint8_t averageBpm,
                        uint8_t maxBpm, const GFXfont *font, int16_t x, int16_t y, int16_t w,
                        int16_t h, uint16_t color, uint16_t background) {
    gfx.setFont(font);
    gfx.setTextColor(color);
    gfx.setTextWrap(false);
    const int16_t digitH = digitHeight(gfx);

    // heading: heart, "Ø 148 bpm   max 176 bpm", centered on the icon
    drawIcon(gfx, Icon::HeartRate, x, y, ICON_SIZE, color, background);
    char text[40];
    snprintf(text, sizeof(text), "\xD8 %u bpm    max %u bpm", averageBpm, maxBpm);
    gfx.setCursor(x + ICON_SIZE + ICON_GAP, y + ICON_SIZE / 2 + digitH / 2);
    gfx.print(text);

    // scale: whole steps around the bars' range, at least MIN_RANGE
    uint8_t lowest = 255, highest = 0;
    for (uint8_t i = 0; i < bars; i++) {
        if (bpm[i] == 0) continue;
        if (bpm[i] < lowest) lowest = bpm[i];
        if (bpm[i] > highest) highest = bpm[i];
    }
    if (highest == 0) return;  // no valid value
    int16_t bottom = (lowest - SCALE_STEP / 2) / SCALE_STEP * SCALE_STEP;  // room below the lowest
    if (bottom < 0) bottom = 0;
    int16_t top = (highest + SCALE_STEP - 1) / SCALE_STEP * SCALE_STEP;
    while (top - bottom < MIN_RANGE) {
        top += SCALE_STEP;
        if (top - bottom < MIN_RANGE && bottom >= SCALE_STEP) bottom -= SCALE_STEP;
    }

    // the labels of the topmost and bottom scale line stick out by half a digit
    const int16_t plotTop = y + ICON_SIZE + PLOT_GAP + digitH / 2;
    const int16_t plotBottom = y + h - 1 - digitH / 2;
    snprintf(text, sizeof(text), "%d", top);
    const int16_t plotX = x + textWidth(gfx, text) + LABEL_GAP;
    const int16_t plotW = x + w - plotX;
    const float pxPerBpm = float(plotBottom - plotTop) / (top - bottom);

    // scale lines with right-aligned labels; the bottom one solid, as the bars' base
    for (int16_t value = bottom; value <= top; value += SCALE_STEP) {
        const int16_t lineY = plotBottom - lroundf((value - bottom) * pxPerBpm);
        snprintf(text, sizeof(text), "%d", value);
        gfx.setCursor(plotX - LABEL_GAP - textWidth(gfx, text), lineY + digitH / 2);
        gfx.print(text);
        if (value == bottom) {
            gfx.drawFastHLine(plotX, lineY, plotW, color);
        } else {
            for (int16_t dotX = plotX; dotX < plotX + plotW; dotX += DOT_SPACING) {
                gfx.drawPixel(dotX, lineY, color);
            }
        }
    }

    // bars: each gets an equal slot of the width, with a gap of a fifth of it (at least 1 px)
    for (uint8_t i = 0; i < bars; i++) {
        if (bpm[i] == 0) continue;
        const int16_t left = plotX + int32_t(i) * plotW / bars;
        const int16_t right = plotX + int32_t(i + 1) * plotW / bars;
        int16_t gap = (right - left) / 5;
        if (gap < 1) gap = 1;
        const int16_t barH = lroundf((bpm[i] - bottom) * pxPerBpm);
        gfx.fillRect(left + gap / 2, plotBottom - barH, right - left - gap, barH, color);
    }
}
