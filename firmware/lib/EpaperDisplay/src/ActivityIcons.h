#ifndef ACTIVITY_ICONS_H
#define ACTIVITY_ICONS_H

#include <Adafruit_GFX.h>

enum class Icon : uint8_t {
    None,   // empty for no-icon
    Error,  // warning triangle with exclamation mark
    Run,
    TrailRun,
    Walk,
    Hike,
    RoadBike,
    GravelBike,
    MountainBike,
    EBike,
    EMountainBike,
    Swim,
    Paddle,  // rowing, kayaking, canoeing, SUP, sailing
    Snow,    // skiing, snowboarding, ice skating
    Weights,
    Generic,      // stopwatch, for every sport type without its own icon
    Achievement,  // medal with star, for Strava achievements
};

// Maps a Strava sport_type (e.g. "TrailRun", "GravelRide") to its icon.
Icon iconForSportType(const char *sportType);

// All Strava sport types with their own icon mapping, e.g. for test screens.
uint8_t sportTypeCount();
const char *sportTypeName(uint8_t index);

// Draws the icon into the square (x, y, size, size). Icons are designed on a
// 40x40 grid and scaled to size; background is used for cut-outs.
void drawIcon(Adafruit_GFX &gfx, Icon icon, int16_t x, int16_t y, int16_t size, uint16_t color,
              uint16_t background);

#endif  // ACTIVITY_ICONS_H
