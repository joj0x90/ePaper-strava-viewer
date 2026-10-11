#include "EpaperDisplay.h"

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#define HEADING_START_X 20
#define HEADING_START_Y 40

#define BODY_START_X 40
#define BODY_START_Y 150

// CS=15, DC=27, RST=26, BUSY=25
static GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(15, 27, 26, 25));

// Only draws into the buffer; call inside a firstPage()/nextPage() loop and never
// change the window here, that would clip everything drawn in the same loop.
static void drawHeading(const char *heading) {
    display.setFont(&FreeSans18pt7b);
    display.setCursor(HEADING_START_X, HEADING_START_Y);
    display.print(heading);
    const int16_t lineY = HEADING_START_Y + 20;
    display.drawLine(HEADING_START_X, lineY, display.width() - HEADING_START_X, lineY, GxEPD_BLACK);
}

void initDisplay() {
    SPI.end();
    SPI.begin(13, 12, 14, 15);  // SCK=13, MISO=12 (unused), MOSI=14, SS=15
    display.init(115200, true, 2, false);
    display.setRotation(0);
}

void drawActivity(StravaActivity act) {
    char distance[16];
    snprintf(distance, sizeof(distance), "%.2f km", act.distance_km);

    char title[35];
    snprintf(title, sizeof(title), "%s: %s", act.type.c_str(), act.name.c_str());

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);

        drawHeading(title);

        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(BODY_START_X, BODY_START_Y);
        display.print(act.type);

        display.setFont(&FreeSans18pt7b);
        display.setCursor(BODY_START_X, 260);
        display.print(act.date);

        display.setFont(&FreeSansBold24pt7b);
        display.setCursor(BODY_START_X, 380);
        display.print(distance);
    } while (display.nextPage());
}

void drawError(const char *message) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        drawHeading("Error:");
        display.setCursor(BODY_START_X, BODY_START_Y);
        display.print(message);
    } while (display.nextPage());
}

void hibernateDisplay() {
    display.hibernate();
}
