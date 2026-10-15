// Icon test firmware: shows every sport type icon on the e-paper.
// Flash with: pio run -e icon-test -t upload
#include <Arduino.h>

#include "EpaperDisplay.h"

void setup() {
    Serial.begin(115200);
    initDisplay();
    drawIconOverviewScreen();
    hibernateDisplay();
    Serial.println("Icon overview drawn");
}

void loop() {}
