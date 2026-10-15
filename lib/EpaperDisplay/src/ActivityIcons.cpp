#include "ActivityIcons.h"

#include <math.h>
#include <string.h>

namespace {

struct SportIcon {
    const char *sportType;
    Icon icon;
};

// Strava sport_type values, see https://developers.strava.com/docs/reference/#api-models-SportType
const SportIcon SPORT_ICONS[] = {
    {"None", Icon::None},
    {"Run", Icon::Run},
    {"VirtualRun", Icon::Run},
    {"TrailRun", Icon::TrailRun},
    {"Walk", Icon::Walk},
    {"Hike", Icon::Hike},
    {"Snowshoe", Icon::Hike},
    {"Ride", Icon::RoadBike},
    {"VirtualRide", Icon::RoadBike},
    {"Velomobile", Icon::RoadBike},
    {"Handcycle", Icon::RoadBike},
    {"GravelRide", Icon::GravelBike},
    {"MountainBikeRide", Icon::MountainBike},
    {"EBikeRide", Icon::EBike},
    {"EMountainBikeRide", Icon::EMountainBike},
    {"Swim", Icon::Swim},
    {"Surfing", Icon::Swim},
    {"Windsurf", Icon::Swim},
    {"Kitesurf", Icon::Swim},
    {"Rowing", Icon::Paddle},
    {"VirtualRow", Icon::Paddle},
    {"Kayaking", Icon::Paddle},
    {"Canoeing", Icon::Paddle},
    {"StandUpPaddling", Icon::Paddle},
    {"Sail", Icon::Paddle},
    {"AlpineSki", Icon::Snow},
    {"BackcountrySki", Icon::Snow},
    {"NordicSki", Icon::Snow},
    {"Snowboard", Icon::Snow},
    {"IceSkate", Icon::Snow},
    {"WeightTraining", Icon::Weights},
    {"Crossfit", Icon::Weights},
    {"Workout", Icon::Weights},
    {"HighIntensityIntervalTraining", Icon::Weights},
};

// Draws shapes given in 40x40 grid units, scaled to the target square.
class Painter {
public:
    Painter(Adafruit_GFX &gfx, int16_t x, int16_t y, int16_t size, uint16_t color, uint16_t bg)
        : gfx_(gfx), x_(x), y_(y), k_(size / 40.0f), fg_(color), bg_(bg) {}

    // Switches between drawing and cutting out (background color).
    void cut(bool on) { color_ = on ? bg_ : fg_; }

    void line(float ax, float ay, float bx, float by, float width) {
        float x0 = px(ax), y0 = py(ay), x1 = px(bx), y1 = py(by), w = width * k_;
        if (w < 1.5f) {
            gfx_.drawLine(lroundf(x0), lroundf(y0), lroundf(x1), lroundf(y1), color());
            return;
        }
        float len = hypotf(x1 - x0, y1 - y0);
        if (len > 0) {
            // Rectangle around the line, made of two triangles
            float nx = -(y1 - y0) / len * w / 2, ny = (x1 - x0) / len * w / 2;
            triPx(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny);
            triPx(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny);
        }
        // Round caps, so connected lines join without gaps
        gfx_.fillCircle(lroundf(x0), lroundf(y0), lroundf(w / 2 - 0.25f), color());
        gfx_.fillCircle(lroundf(x1), lroundf(y1), lroundf(w / 2 - 0.25f), color());
    }

    // Connected line segments through n points {x0, y0, x1, y1, ...}.
    void polyline(const float *pts, uint8_t n, float width) {
        for (uint8_t i = 0; i + 1 < n; i++) {
            line(pts[2 * i], pts[2 * i + 1], pts[2 * i + 2], pts[2 * i + 3], width);
        }
    }

    // Filled polygon, triangulated as a fan around (cx, cy); the polygon must
    // be star-shaped as seen from that point.
    void polygon(const float *pts, uint8_t n, float cx, float cy) {
        for (uint8_t i = 0; i < n; i++) {
            uint8_t j = (i + 1) % n;
            tri(cx, cy, pts[2 * i], pts[2 * i + 1], pts[2 * j], pts[2 * j + 1]);
        }
    }

    void tri(float ax, float ay, float bx, float by, float cx, float cy) {
        triPx(px(ax), py(ay), px(bx), py(by), px(cx), py(cy));
    }

    void circle(float cx, float cy, float r) {
        gfx_.fillCircle(lroundf(px(cx)), lroundf(py(cy)), lroundf(r * k_), color());
    }

    void ring(float cx, float cy, float r, float width) {
        circle(cx, cy, r);
        uint16_t c = color_;
        color_ = bg_;
        circle(cx, cy, r - width);
        color_ = c;
    }

    void rect(float x, float y, float w, float h, float radius) {
        gfx_.fillRoundRect(lroundf(px(x)), lroundf(py(y)), lroundf(w * k_), lroundf(h * k_),
                           lroundf(radius * k_), color());
    }

private:
    float px(float v) const { return x_ + v * k_; }
    float py(float v) const { return y_ + v * k_; }
    uint16_t color() const { return color_; }

    void triPx(float ax, float ay, float bx, float by, float cx, float cy) {
        gfx_.fillTriangle(lroundf(ax), lroundf(ay), lroundf(bx), lroundf(by), lroundf(cx),
                          lroundf(cy), color());
    }

    Adafruit_GFX &gfx_;
    int16_t x_, y_;
    float k_;
    uint16_t fg_, bg_;
    uint16_t color_ = fg_;
};

void drawError(Painter &p) {
    const float triangle[] = {20, 2, 39, 36, 1, 36};
    p.polygon(triangle, 3, 20, 24);
    p.cut(true);
    p.line(20, 13, 20, 24, 4.5f);
    p.circle(20, 30.5f, 2.5f);
    p.cut(false);
}

// Running shoe seen from the side, toe pointing right.
void drawShoe(Painter &p, bool lugs) {
    const float upper[] = {4, 30, 5, 14, 13, 16, 22, 21, 32, 23, 37, 27, 37, 30};
    p.polygon(upper, 7, 18, 26);
    p.rect(2, 29, 36, 5, 2);
    p.cut(true);
    p.line(3, 29, 37, 29, 1.5f);   // gap between upper and sole
    p.line(15, 19, 13, 23, 1.5f);  // laces
    p.line(19, 20.5f, 17, 24.5f, 1.5f);
    p.line(23, 22, 21, 26, 1.5f);
    p.cut(false);
    if (lugs) {
        for (float x = 4; x < 37; x += 6) p.rect(x, 33, 3, 4, 0);
    }
}

void drawMountains(Painter &p) {
    p.tri(19, 16, 27, 5, 35, 16);
    p.tri(28, 16, 33, 9, 39, 16);
}

// Bare foot sole with toes; mirror flips the big toe to the other side.
void drawFoot(Painter &p, float x, float y, bool mirror) {
    const float side = mirror ? -1 : 1, mid = x + 5.5f;
    p.rect(x, y + 6, 11, 12, 5.5f);            // ball of the foot
    p.rect(x + 1.5f, y + 14, 8, 10, 4);        // arch and heel
    p.circle(mid + side * 3.5f, y + 3, 2.3f);  // big toe
    p.circle(mid + side * 0.2f, y + 2.2f, 1.5f);
    p.circle(mid - side * 2.6f, y + 3, 1.4f);
    p.circle(mid - side * 4.8f, y + 4.6f, 1.2f);
}

void drawFootprints(Painter &p) {
    drawFoot(p, 4, 15, false);
    drawFoot(p, 23, 1, true);
}

// High hiking boot with chunky sole.
void drawBoot(Painter &p) {
    const float upper[] = {6, 30, 7, 6, 17, 6, 18, 19, 31, 22, 37, 26, 37, 30};
    p.polygon(upper, 7, 14, 22);
    p.rect(3, 29, 35, 6, 2);
    p.cut(true);
    p.line(4, 29, 37, 29, 1.5f);
    p.line(12, 9, 16, 11, 1.5f);  // laces
    p.line(12, 13, 16, 15, 1.5f);
    p.line(12, 17, 17, 19.5f, 1.5f);
    p.line(7, 23, 14, 23, 1.5f);  // ankle padding
    p.cut(false);
    for (float x = 4; x < 37; x += 6) p.rect(x, 34, 3, 3, 0);
}

enum class BikeStyle : uint8_t { Road, Gravel, Mountain };

void drawBike(Painter &p, BikeStyle style, bool electric) {
    const float rearX = 9.5f, frontX = 30.5f, hubY = 27, r = 8.5f;
    const float tire = style == BikeStyle::Road ? 1.8f : style == BikeStyle::Gravel ? 3 : 4;
    p.ring(rearX, hubY, r, tire);
    p.ring(frontX, hubY, r, tire);
    if (style == BikeStyle::Mountain) {
        // knobs around the tires
        for (uint8_t i = 0; i < 12; i++) {
            float a = i * (float)M_PI / 6;
            p.circle(rearX + cosf(a) * r, hubY + sinf(a) * r, 1.2f);
            p.circle(frontX + cosf(a) * r, hubY + sinf(a) * r, 1.2f);
        }
    }

    const float bbX = 19, bbY = 27;         // bottom bracket
    const float seatX = 15.5f, seatY = 15;  // top of seat tube
    const float headX = 28, headY = 15;     // top of head tube
    const float frame[] = {rearX, hubY, bbX, bbY, seatX, seatY, rearX, hubY};
    p.polyline(frame, 4, 2);
    const float mainTriangle[] = {seatX, seatY, headX, headY, bbX, bbY};
    p.polyline(mainTriangle, 3, 2);
    p.line(headX, headY, frontX, hubY, 2);  // fork
    p.line(seatX, seatY, 15, 12, 2);        // seat post
    p.line(11.5f, 11.5f, 18, 11.5f, 2.2f);  // saddle
    p.line(headX, headY, 28.5f, 11.5f, 2);  // stem

    if (style == BikeStyle::Mountain) {
        p.line(26, 11, 33, 10.5f, 2);  // flat bar
    } else {
        const float dropBar[] = {28.5f, 11.5f, 33, 11.5f, 34.5f, 14, 33, 17};
        p.polyline(dropBar, 4, 2);
    }

    if (style == BikeStyle::Gravel) {
        // stones under the wheels
        p.circle(18, 38, 1.4f);
        p.circle(21.5f, 37.5f, 1.8f);
        p.circle(2.5f, 38, 1.2f);
        p.circle(37.5f, 38, 1.4f);
    }

    if (electric) {
        // lightning bolt above the top tube
        p.tri(23, 0, 18, 7, 22, 7);
        p.tri(22, 5, 21, 12, 25.5f, 5);
        p.tri(20, 5, 25.5f, 5, 22, 7);
    }
}

void drawWaves(Painter &p) {
    for (float y = 10; y <= 31; y += 10.5f) {
        float prevX = 2, prevY = y;
        for (float x = 3; x <= 38; x += 1) {
            float wy = y + sinf((x - 2) * 2 * (float)M_PI / 12) * 3;
            p.line(prevX, prevY, x, wy, 3);
            prevX = x;
            prevY = wy;
        }
    }
}

void drawBoat(Painter &p) {
    // kayak with a double-bladed paddle above it
    p.line(8, 5, 32, 17, 2.2f);
    p.line(5, 3.5f, 10, 6, 5);
    p.line(30, 16, 35, 18.5f, 5);
    const float hull[] = {1, 22, 39, 22, 32, 28, 8, 28};
    p.polygon(hull, 4, 20, 25);
    p.circle(20, 19, 3.5f);  // paddler
    float prevX = 2, prevY = 34;
    for (float x = 3; x <= 38; x += 1) {
        float wy = 34 + sinf((x - 2) * 2 * (float)M_PI / 12) * 2;
        p.line(prevX, prevY, x, wy, 2.5f);
        prevX = x;
        prevY = wy;
    }
}

void drawSnowflake(Painter &p) {
    for (uint8_t i = 0; i < 6; i++) {
        float a = i * (float)M_PI / 3 - (float)M_PI / 2;
        float ex = 20 + cosf(a) * 18, ey = 20 + sinf(a) * 18;
        p.line(20, 20, ex, ey, 3);
        // V-shaped branch on each arm
        float bx = 20 + cosf(a) * 11, by = 20 + sinf(a) * 11;
        for (int8_t side = -1; side <= 1; side += 2) {
            float ba = a + side * (float)M_PI / 4;
            p.line(bx, by, bx + cosf(ba) * 6, by + sinf(ba) * 6, 2.5f);
        }
    }
}

void drawDumbbell(Painter &p) {
    p.line(8, 20, 32, 20, 3.5f);
    p.rect(7, 8, 6, 24, 2);
    p.rect(27, 8, 6, 24, 2);
    p.rect(2, 12, 5, 16, 2);
    p.rect(33, 12, 5, 16, 2);
}

void drawStopwatch(Painter &p) {
    p.rect(16, 1, 8, 4, 1);  // button
    p.line(20, 4, 20, 8, 3);
    p.line(31, 9, 33.5f, 6.5f, 3);  // side button
    p.ring(20, 23, 16, 3);
    p.line(20, 23, 20, 13, 2.5f);
    p.line(20, 23, 26, 27, 2.5f);
    p.circle(20, 23, 2.2f);
}

// Medal on a ribbon with a star in the middle.
void drawMedal(Painter &p) {
    const float leftStrap[] = {7, 1, 15, 1, 22, 18, 15, 20};
    const float rightStrap[] = {25, 1, 33, 1, 25, 20, 18, 18};
    p.polygon(leftStrap, 4, 15, 10);
    p.polygon(rightStrap, 4, 25, 10);
    p.ring(20, 27, 12.5f, 2.5f);

    float star[20];
    for (uint8_t i = 0; i < 10; i++) {
        float a = i * (float)M_PI / 5 - (float)M_PI / 2;
        float r = i % 2 == 0 ? 7.5f : 3.2f;  // alternating outer and inner points
        star[2 * i] = 20 + cosf(a) * r;
        star[2 * i + 1] = 27.5f + sinf(a) * r;
    }
    p.polygon(star, 10, 20, 27.5f);
}

// Thumbs up: cuff on the left, fist with the thumb pointing up.
void drawThumbsUp(Painter &p) {
    p.rect(2, 17, 7, 21, 1.5f);  // cuff
    p.rect(11, 17, 14, 21, 3);   // back of the hand
    const float thumb[] = {11, 18, 15, 9, 17, 3, 22, 3, 22, 10, 21, 17};
    p.polygon(thumb, 6, 17, 12);
    p.circle(19.5f, 4.5f, 2.5f);
    // fingers, getting shorter towards the bottom
    p.rect(19, 15, 19, 6, 3);
    p.rect(19, 21, 18, 6, 3);
    p.rect(19, 27, 17, 5.5f, 2.75f);
    p.rect(19, 32.5f, 15, 5.5f, 2.75f);
}

// Speech bubble with a tail at the bottom left.
void drawSpeechBubble(Painter &p) {
    p.rect(1, 3, 38, 26, 6);
    p.tri(8, 27, 18, 27, 6, 37);
}

// Arc of a circle from angle a0 to a1 (radians, 0 = right, clockwise as y points down).
void drawArc(Painter &p, float cx, float cy, float r, float a0, float a1, float width) {
    const uint8_t steps = 16;
    float prevX = cx + cosf(a0) * r, prevY = cy + sinf(a0) * r;
    for (uint8_t i = 1; i <= steps; i++) {
        float a = a0 + (a1 - a0) * i / steps;
        float x = cx + cosf(a) * r, y = cy + sinf(a) * r;
        p.line(prevX, prevY, x, y, width);
        prevX = x;
        prevY = y;
    }
}

// Lowercase letters drawn as strokes, so they scale with the icon. (x, y) is the left end of
// the baseline, h the x-height; m and n also need the stroke width w.
void drawLetterM(Painter &p, float x, float y, float h, float w) {
    const float r = h * 0.3f, pi = (float)M_PI;
    p.line(x, y - h, x, y, w);
    for (uint8_t i = 0; i < 2; i++) {
        const float cx = x + r + 2 * r * i;
        drawArc(p, cx, y - h + r, r, pi, 2 * pi, w);
        p.line(cx + r, y - h + r, cx + r, y, w);
    }
}

void drawLetterN(Painter &p, float x, float y, float h, float r, float w) {
    p.line(x, y - h, x, y, w);
    drawArc(p, x + r, y - h + r, r, (float)M_PI, 2 * (float)M_PI, w);
    p.line(x + 2 * r, y - h + r, x + 2 * r, y, w);
}

// Lowercase s with x-height 16 and baseline 27, for s_{avg} and s_{max}; the subscript goes
// right of it with x-height 7 and baseline 34.
void drawLetterS(Painter &p) {
    const float pi = (float)M_PI;
    drawArc(p, 8, 15, 4, -pi / 6, -1.5f * pi, 3);
    drawArc(p, 8, 23, 4, -pi / 2, pi * 5 / 6, 3);
}

// Letter a of the subscript, with its bowl centered at cx.
void drawSubscriptA(Painter &p, float cx, float w) {
    const float r = 3.5f;  // outer radius of the bowl
    p.ring(cx, 30.5f, r, w);
    p.line(cx + r - w / 2, 27, cx + r - w / 2, 34, w);
}

// s_{avg}: average speed, like in LaTeX.
void drawAvgSpeed(Painter &p) {
    const float pi = (float)M_PI, w = 2, r = 3.5f;
    drawLetterS(p);
    drawSubscriptA(p, 16.5f, w);
    p.line(22.5f, 27, 25.5f, 34, w);  // v
    p.line(25.5f, 34, 28.5f, 27, w);
    p.ring(33, 30.5f, r, w);  // g
    p.line(33 + r - w / 2, 27, 33 + r - w / 2, 36, w);
    drawArc(p, 33, 36, r - w / 2, 0, pi * 0.9f, w);
}

// s_{max}: maximum speed, like in LaTeX.
void drawMaxSpeed(Painter &p) {
    const float w = 2;
    drawLetterS(p);
    drawLetterM(p, 14.5f, 34, 7, w);
    drawSubscriptA(p, 28, w);
    p.line(33.5f, 27, 38.5f, 34, w);  // x
    p.line(38.5f, 27, 33.5f, 34, w);
}

// Scale of a ruler: baseline with long, medium and short tick marks.
void drawRuler(Painter &p) {
    // rectangles instead of lines: square ends without the bumps of round caps
    p.rect(2, 27, 36, 2.5f, 0);
    for (uint8_t i = 0; i < 9; i++) {
        const float x = 2 + i * 4.25f;
        const float top = i % 4 == 0 ? 12 : i % 2 == 0 ? 18 : 22;
        p.rect(x, top, 2, 28 - top, 0);
    }
}

// Flame with an inner flame cut out.
void drawFlame(Painter &p) {
    const float outer[] = {21, 1, 28, 9, 32, 17, 33, 26, 7, 26, 7, 18, 10, 9, 14, 15};
    p.polygon(outer, 8, 20, 20);
    p.circle(20, 26, 13);
    p.cut(true);
    const float inner[] = {20, 17, 25, 25, 26, 31, 14, 31, 15, 25};
    p.polygon(inner, 5, 20, 27);
    p.circle(20, 31, 6);
    p.cut(false);
}

// Heart: two circles on a triangle with its tip at the bottom.
void drawHeart(Painter &p) {
    p.circle(13, 15, 9);
    p.circle(27, 15, 9);
    const float body[] = {4.6f, 18, 20, 12, 35.4f, 18, 20, 35};
    p.polygon(body, 4, 20, 20);
}

// Bell: dome and flaring body on a rim, with the knob on top and the clapper below.
void drawBell(Painter &p) {
    p.circle(20, 4.5f, 3);
    p.circle(20, 16, 10);
    const float body[] = {10, 16, 30, 16, 34, 29, 6, 29};
    p.polygon(body, 4, 20, 22);
    p.rect(3, 28, 34, 4.5f, 2);
    p.circle(20, 35.5f, 4);
}

}  // namespace

uint8_t sportTypeCount() {
    return sizeof(SPORT_ICONS) / sizeof(SPORT_ICONS[0]);
}

const char *sportTypeName(uint8_t index) {
    return index < sportTypeCount() ? SPORT_ICONS[index].sportType : "";
}

Icon iconForSportType(const char *sportType) {
    for (const SportIcon &entry : SPORT_ICONS) {
        if (strcmp(entry.sportType, sportType) == 0) return entry.icon;
    }
    return Icon::Generic;
}

void drawIcon(Adafruit_GFX &gfx, Icon icon, int16_t x, int16_t y, int16_t size, uint16_t color,
              uint16_t background) {
    Painter p(gfx, x, y, size, color, background);
    switch (icon) {
        case Icon::None:  break;
        case Icon::Error: drawError(p); break;
        case Icon::Run:   drawShoe(p, false); break;
        case Icon::TrailRun:
            drawShoe(p, true);
            drawMountains(p);
            break;
        case Icon::Walk:          drawFootprints(p); break;
        case Icon::Hike:          drawBoot(p); break;
        case Icon::RoadBike:      drawBike(p, BikeStyle::Road, false); break;
        case Icon::GravelBike:    drawBike(p, BikeStyle::Gravel, false); break;
        case Icon::MountainBike:  drawBike(p, BikeStyle::Mountain, false); break;
        case Icon::EBike:         drawBike(p, BikeStyle::Road, true); break;
        case Icon::EMountainBike: drawBike(p, BikeStyle::Mountain, true); break;
        case Icon::Swim:          drawWaves(p); break;
        case Icon::Paddle:        drawBoat(p); break;
        case Icon::Snow:          drawSnowflake(p); break;
        case Icon::Weights:       drawDumbbell(p); break;
        case Icon::Generic:       drawStopwatch(p); break;
        case Icon::Achievement:   drawMedal(p); break;
        case Icon::Kudos:         drawThumbsUp(p); break;
        case Icon::Comment:       drawSpeechBubble(p); break;
        case Icon::AvgSpeed:      drawAvgSpeed(p); break;
        case Icon::MaxSpeed:      drawMaxSpeed(p); break;
        case Icon::Calories:      drawFlame(p); break;
        case Icon::Distance:      drawRuler(p); break;
        case Icon::HeartRate:     drawHeart(p); break;
        case Icon::Bell:          drawBell(p); break;
    }
}
