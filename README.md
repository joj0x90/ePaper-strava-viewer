# ePaper-strava-viewer

Show your Strava activities on a Waveshare 7.5" e-Paper display driven by the Waveshare e-Paper ESP32 Driver Board.

## Hardware

| Part | Notes |
| --- | --- |
| Waveshare 7.5" e-Paper (V2, 800×480, black/white) | Raw panel with FPC ribbon cable |
| Waveshare e-Paper ESP32 Driver Board | ESP32-WROOM-32, CP2102 USB-UART, onboard 24-pin FPC connector |
| Micro-USB cable | Must be a data cable, not charge-only |

## Software prerequisites

1. **VS Code**: <https://code.visualstudio.com/>
2. **PlatformIO IDE extension**: in VS Code install the Extension.
3. **Git**, to clone this repository.
4. **USB-UART driver** (CP2102):
   - **Linux**: the driver is built into the kernel. Your user needs access to the serial port:
     ```sh
     # Arch Linux
     sudo usermod -aG uucp $USER
     # Debian/Ubuntu
     sudo usermod -aG dialout $USER
     ```
     Log out and back in afterwards. PlatformIO also recommends installing its udev rules: <https://docs.platformio.org/en/latest/core/installation/udev-rules.html>
   - **Windows/macOS**: install the CP210x driver from Silicon Labs: <https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers>


### 1. PlatformIO configuration

`platformio.ini`:

```ini
[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
board_build.partitions = huge_app.csv

lib_deps =
    zinggjm/GxEPD2
    bblanchon/ArduinoJson

[env:icon-test]
extends = env:esp32dev
build_src_filter = -<*> +<../tools/icon_overview/device/>
```

`esp32dev` is the normal firmware; `icon-test` is a test firmware, see [Testing](#testing).

- **GxEPD2**: e-Paper driver. The 7.5" V2 panel uses the `GxEPD2_750_T7` class.
- **ArduinoJson**: parses the responses from the Strava API.

Minimal display initialisation for this board:

```cpp
#include <SPI.h>
#include <GxEPD2_BW.h>

// CS=15, DC=27, RST=26, BUSY=25
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(15, 27, 26, 25));

void setup() {
  SPI.end();
  SPI.begin(13, 12, 14, 15);  // SCK=13, MISO=12 (unused), MOSI=14, SS=15
  display.init(115200, true, 2, false);
  // ... draw
}

void loop() {
  // required by the Arduino framework, even if empty
}
```

### 2. Strava API credentials

1. Log in to Strava and go to <https://www.strava.com/settings/api>.
2. Create an application. Set **Authorization Callback Domain** to `localhost`.
3. Note the **Client ID** and **Client Secret**.
4. Open this URL in your browser (replace `YOUR_CLIENT_ID`) and authorize the app:
   ```
   https://www.strava.com/oauth/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=http://localhost&approval_prompt=force&scope=read,activity:read_all
   ```
5. The browser redirects to `http://localhost/?state=&code=XXXX&scope=...`. The page itself won't load, which is expected. Copy the `code` value from the address bar.
6. Exchange the code for a refresh token:
   ```sh
   curl -X POST https://www.strava.com/oauth/token \
     -d client_id=YOUR_CLIENT_ID \
     -d client_secret=YOUR_CLIENT_SECRET \
     -d code=XXXX \
     -d grant_type=authorization_code
   ```
   Copy the `refresh_token` from the JSON response. The ESP32 uses it to request short-lived access tokens on its own.

### 3. Secrets file

Create `firmware/include/secrets.h`:

```cpp
#pragma once

#define WIFI_SSID            "your-wifi-name"
#define WIFI_PASSWORD        "your-wifi-password"

#define STRAVA_CLIENT_ID     "12345"
#define STRAVA_CLIENT_SECRET "your-client-secret"
#define STRAVA_REFRESH_TOKEN "your-refresh-token"
```

Use it in the code with `#include "secrets.h"`.

### 4. Build and upload

1. Connect the driver board via USB.
2. In the PlatformIO toolbar at the bottom of VS Code:
   - ✓ **Build** (`Ctrl+Alt+B`)
   - → **Upload** (`Ctrl+Alt+U`)
   - 🔌 **Serial Monitor**: shows log output at 115200 baud
3. Or from the PlatformIO terminal:
   ```sh
   pio run -t upload
   pio device monitor
   ```

If the upload hangs at `Connecting........`, hold the **BOOT** button on the driver board until the upload starts, then press **EN/RST** after it finishes.

## Testing

### Activity icons

The heading shows an icon for the activity's `sport_type`.

Both test methods draw the same overview: one grid cell per sport type with its icon and name, plus the fallback icon `(other)` and the error icon `(error)`.

#### On the PC (no flashing)

```sh
firmware/tools/icon_overview/pc/run.sh
```

Compiles the icon code for the PC and writes `firmware/tools/icon_overview/pc/build/icon_overview.png` (800×480, like the display). Fast to repeat while designing icons.

Requirements:
- `g++`
- the libraries PlatformIO downloads: run `pio run` in `firmware/` once
- Python 3 with Pillow (`pip install pillow`) for the PNG; without it you get `icon_overview.pbm`, which most image viewers open as well

The script uses small stand-ins for the Arduino core (`firmware/tools/icon_overview/pc/stubs/`) so Adafruit GFX compiles on the PC. The drawing code is the same as on the ESP32, so the image matches the panel pixel for pixel.

#### On the e-paper

```sh
cd firmware
pio run -e icon-test -t upload
```

Flashes a test firmware (`firmware/tools/icon_overview/device/main.cpp`) that draws the overview once and puts the display to sleep. Use this to check the final look on the panel. Afterwards flash the normal firmware again with `pio run -t upload`.

In VS Code you can switch the environment in the PlatformIO toolbar (*Switch PlatformIO Project Environment*) to `env:icon-test` and use Upload as usual.

### Error codes

WiFi and Strava errors are shown on the display and logged with details on the serial monitor (`pio device monitor`), for example:

```
WiFi error 1 (status 1): WiFi network not found
Strava error 1 (HTTP 401): Strava login failed
{"message":"Authorization Error", ...}
```

The numbers are the values of `WifiErrorCode` (`firmware/lib/WifiConnect/src/WifiConnect.h`) and `StravaErrorCode` (`firmware/lib/StravaApi/src/StravaApi.h`). For Strava errors the last line is the response body or the JSON parser error.

## Troubleshooting

| Problem | Fix |
| --- | --- |
| Red squiggles in VS Code (`Arduino.h` not found etc.) although the build works | Open the `firmware/` folder in VS Code. Code analysis must come from the Microsoft C/C++ extension (`ms-vscode.cpptools`), which reads PlatformIO's generated `c_cpp_properties.json`. `firmware/.vscode/settings.json` enables it and disables clangd for this project. Then run *PlatformIO: Rebuild IntelliSense Index* |
| No serial port shows up | Use a data USB cable; check the CP2102 driver and your user's group membership (`uucp`/`dialout`) |
| `Permission denied: /dev/ttyUSB0` | Add your user to the serial group (see above) and log in again |
| Display stays white or shows noise | Check that the ribbon cable is seated properly; toggle the Display Config switch between A and B; make sure you use `GxEPD2_750_T7` (V2) and not `GxEPD2_750` (V1) |
| Display is stuck in "busy" (timeout in the log) | Wrong pins: check the BUSY/RST pin mapping and the `SPI.begin(13, 12, 14, 15)` remap |
| Ghosting after many partial refreshes | Do a full refresh regularly (e.g. every 10th update) |
| Strava returns `401 Unauthorized` | Refresh token is invalid or the scope is missing `activity:read_all`; repeat step 2 |
| WiFi doesn't connect | Check SSID/password; use a 2.4 GHz network |
