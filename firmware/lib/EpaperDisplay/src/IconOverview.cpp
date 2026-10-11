#include "IconOverview.h"

#include <string.h>

#include "ActivityIcons.h"

namespace {

const uint8_t COLUMNS = 8;
const int16_t ICON_SIZE = 48;
const int16_t CHAR_W = 6, LINE_H = 10;  // built-in 5x7 font at size 1

// Icons that don't belong to a sport type, shown after the sport types
struct ExtraIcon {
    Icon icon;
    const char *label;
};
const ExtraIcon EXTRA_ICONS[] = {
    {Icon::Generic, "(other)"},
    {Icon::Error, "(error)"},
    {Icon::Achievement, "(achievement)"},
};
const uint8_t EXTRA_CELLS = sizeof(EXTRA_ICONS) / sizeof(EXTRA_ICONS[0]);

// Prints name centered below the icon, wrapped at camelCase word boundaries
// ("HighIntensityIntervalTraining" -> "HighIntensity" / "IntervalTraining").
void printWrapped(Adafruit_GFX &gfx, const char *name, int16_t centerX, int16_t y,
                  int16_t maxWidth) {
    const size_t maxChars = maxWidth / CHAR_W;
    size_t len = strlen(name), start = 0;
    while (start < len) {
        size_t end = len;
        if (end - start > maxChars) {
            // last word start (uppercase letter) that still fits on this line
            end = start + maxChars;
            while (end > start + 1 && !(name[end] >= 'A' && name[end] <= 'Z')) end--;
            if (end == start + 1) end = start + maxChars;  // no boundary: hard break
        }
        char line[32];
        size_t n = end - start < sizeof(line) - 1 ? end - start : sizeof(line) - 1;
        memcpy(line, name + start, n);
        line[n] = '\0';
        gfx.setCursor(centerX - n * CHAR_W / 2, y);
        gfx.print(line);
        y += LINE_H;
        start = end;
    }
}

void drawCell(Adafruit_GFX &gfx, uint8_t index, Icon icon, const char *name, uint16_t color,
              uint16_t background) {
    const uint8_t rows = (sportTypeCount() + EXTRA_CELLS + COLUMNS - 1) / COLUMNS;
    const int16_t cellW = gfx.width() / COLUMNS, cellH = gfx.height() / rows;
    const int16_t x = (index % COLUMNS) * cellW, y = (index / COLUMNS) * cellH;

    gfx.drawRect(x, y, cellW + 1, cellH + 1, color);
    drawIcon(gfx, icon, x + (cellW - ICON_SIZE) / 2, y + 8, ICON_SIZE, color, background);
    printWrapped(gfx, name, x + cellW / 2, y + ICON_SIZE + 16, cellW - 6);
}

}  // namespace

void drawIconOverview(Adafruit_GFX &gfx, uint16_t color, uint16_t background) {
    gfx.fillScreen(background);
    gfx.setFont(nullptr);
    gfx.setTextSize(1);
    gfx.setTextColor(color);
    gfx.setTextWrap(false);

    uint8_t i = 0;
    for (; i < sportTypeCount(); i++) {
        drawCell(gfx, i, iconForSportType(sportTypeName(i)), sportTypeName(i), color, background);
    }
    for (const ExtraIcon &extra : EXTRA_ICONS) {
        drawCell(gfx, i++, extra.icon, extra.label, color, background);
    }
}
