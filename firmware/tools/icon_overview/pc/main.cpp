// Renders the icon overview test screen (same as the icon-test firmware) to a PBM image.
#include <stdio.h>

#include "IconOverview.h"

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "icon_overview.pbm";
    GFXcanvas1 canvas(800, 480);  // 7.5" panel resolution
    drawIconOverview(canvas, 1, 0);

    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return 1;
    }
    // PBM: 1 bit per pixel, 1 = black; same bit layout as GFXcanvas1
    fprintf(f, "P4\n%d %d\n", canvas.width(), canvas.height());
    fwrite(canvas.getBuffer(), 1, (canvas.width() + 7) / 8 * canvas.height(), f);
    fclose(f);
    return 0;
}
