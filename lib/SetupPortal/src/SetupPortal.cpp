#include "SetupPortal.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AppIcon.h"
#include "OtaUpdate.h"
#include "Settings.h"
#include "StravaApi.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"  // set by tools/version.py
#endif

static const IPAddress AP_IP(192, 168, 4, 1);
static const char *MDNS_HOSTNAME = "strava-viewer";  // see SETUP_MDNS_URL
static const uint8_t MAX_LISTED_NETWORKS = 20;
// After saving, keep serving a moment, so the answer page reaches the browser before WiFi stops
static const uint32_t ANSWER_DELAY_MS = 1000;

// Where Strava's authorization page sends the browser back to. Its domain must be the
// Authorization Callback Domain of the Strava app: strava-viewer.local leads straight back to the
// device; localhost leads to an error page, whose address is then pasted into the setup page.
static const char *STRAVA_CALLBACK_URL = SETUP_MDNS_URL "strava/callback";
static const char *STRAVA_LOCALHOST_URL = "http://localhost/";
// Listing activities needs activity:read; activity:read_all adds the private ones. Both are
// requested, so unchecking "private activities" at Strava still leaves the public ones.
static const char *STRAVA_SCOPE = "read,activity:read,activity:read_all";

// Placeholders in %NAME% are filled in by handleRoot(). The timer counts down in the browser,
// starting from the remaining time when the page was loaded.
static const char SETTINGS_PAGE[] = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Strava Viewer setup</title>
<style>
body{font-family:system-ui,sans-serif;max-width:32rem;margin:0 auto;padding:1rem;color:#111;background:#fff}
h1{font-size:1.4rem;margin:.2rem 0 .8rem}
h2{font-size:1.1rem;margin:1.6rem 0 .4rem}
.timer{background:#f2f2f2;border-radius:.5rem;padding:.6rem .8rem;font-weight:600}
table{width:100%;border-collapse:collapse}
td{padding:.35rem 0;border-bottom:1px solid #ddd;vertical-align:top}
td:first-child{color:#555;width:40%}
label{display:block;margin:.8rem 0 .2rem;font-weight:600}
input{width:100%;box-sizing:border-box;padding:.6rem;font-size:1rem;border:1px solid #999;border-radius:.4rem}
small{color:#555}
button{width:100%;padding:.8rem;font-size:1rem;font-weight:600;border:0;border-radius:.4rem;margin-top:1.2rem;background:#fc4c02;color:#fff}
button.danger{background:#fff;color:#b00020;border:2px solid #b00020}
button.secondary{background:#fff;color:#fc4c02;border:2px solid #fc4c02}
.banner{border-radius:.5rem;padding:.6rem .8rem}
.banner.ok{background:#e3f4e6}
.banner.error{background:#fde3e3}
.note{background:#f2f2f2;border-radius:.5rem;padding:.6rem .8rem}
details{margin-top:1.2rem}
summary{cursor:pointer;font-weight:600}
ol{padding-left:1.2rem}
li{margin:.6rem 0}
.copyrow{display:flex;gap:.4rem}
.copyrow input{background:#f2f2f2}
button.copy{width:auto;margin:0;padding:.6rem .8rem;background:#fff;color:#fc4c02;border:2px solid #fc4c02}
a.button{display:block;box-sizing:border-box;text-align:center;text-decoration:none;padding:.8rem;font-weight:600;border-radius:.4rem;margin-top:.4rem;color:#fc4c02;border:2px solid #fc4c02}
</style></head><body>
<h1>Strava Viewer setup</h1>
<p class="timer" id="timer">Setup mode ends in %MINUTES% minutes</p>
%BANNER%

<h2>Device</h2>
<table>
<tr><td>Firmware</td><td>%FIRMWARE%</td></tr>
<tr><td>Battery</td><td>%BATTERY%</td></tr>
<tr><td>Connection</td><td>%CONNECTION%</td></tr>
<tr><td>Strava</td><td>%STRAVA%</td></tr>
</table>
%UPDATE%

<form method="post" action="/save">
<!-- Enter in a field presses the first button: Save, not Connect with Strava -->
<button type="submit" tabindex="-1" aria-hidden="true" style="position:absolute;left:-9999px">Save</button>
<h2>WiFi</h2>
<label for="ssid">Network name</label>
<input id="ssid" name="ssid" list="networks" value="%SSID%" maxlength="32" required autocomplete="off">
<datalist id="networks">%NETWORKS%</datalist>
<label for="password">Password</label>
<input id="password" name="password" type="password" maxlength="63" placeholder="%PASSWORD_HINT%">

<h2>Strava</h2>
<details%GUIDE_OPEN%><summary>Create the Strava API application (once)</summary>
<ol>
<li><a href="https://www.strava.com/settings/api" target="_blank" rel="noopener">Open
strava.com/settings/api</a> and log in.</li>
<li>Fill in the form with these values:
<label for="appName">Application Name</label>
<div class="copyrow"><input id="appName" readonly value="ePaper Strava Viewer"><button type="button" class="copy">Copy</button></div>
<label>Category</label>
<small>Choose <b>Visualizer</b>. Leave <b>Club</b> empty.</small>
<label for="appWebsite">Website</label>
<div class="copyrow"><input id="appWebsite" readonly value="https://github.com/joj0x90/ePaper-strava-viewer"><button type="button" class="copy">Copy</button></div>
<label for="appDescription">Application Description</label>
<div class="copyrow"><input id="appDescription" readonly value="Shows my latest activity on an e-paper display."><button type="button" class="copy">Copy</button></div>
<label for="appDomain">Authorization Callback Domain</label>
<div class="copyrow"><input id="appDomain" readonly value="strava-viewer.local"><button type="button" class="copy">Copy</button></div>
</li>
<li>Strava asks for an icon next. Download it here and upload it there:
<a class="button" href="/app-icon.png" download="strava-viewer-icon.png">Download the app icon</a></li>
<li>Copy the <b>Client ID</b> and <b>Client Secret</b> into the fields below. Or copy the
whole <i>My API Application</i> page (with the secret shown) and paste it into one of the two
fields: both are filled in.</li>
</ol>
</details>
<small>Fields left empty keep their stored value.</small>
<label for="clientId">Client ID</label>
<input id="clientId" name="clientId" value="%CLIENT_ID%" inputmode="numeric" autocomplete="off">
<label for="clientSecret">Client secret</label>
<input id="clientSecret" name="clientSecret" type="password" placeholder="%SECRET_HINT%" autocomplete="off">
%STRAVA_CONNECT%
<details><summary>Enter the refresh token by hand</summary>
<label for="refreshToken">Refresh token</label>
<input id="refreshToken" name="refreshToken" type="password" placeholder="%TOKEN_HINT%" autocomplete="off">
</details>

<button type="submit">Save and show the activity</button>
</form>

<h2>Reset</h2>
<small>Deletes all settings, including the WiFi and Strava credentials, and restarts the device.</small>
<form method="post" action="/reset" onsubmit="return confirm('Delete all settings and restart the device?')">
<button class="danger" type="submit">Reset device</button>
</form>

<script>
let left = %SECONDS%;
const timer = document.getElementById('timer');
function tick() {
  if (left <= 0) {
    timer.textContent = 'Setup mode has ended. Press the button on the device to start it again.';
    return;
  }
  timer.textContent = 'Setup mode ends in ' + Math.floor(left / 60) + ':' +
                      String(left % 60).padStart(2, '0');
  left--;
  setTimeout(tick, 1000);
}
tick();

// The page is plain http, where the clipboard API is missing: copy the selected field instead.
// If even that fails, the value stays selected for copying by hand.
for (const button of document.querySelectorAll('button.copy')) {
  button.onclick = () => {
    const input = button.previousElementSibling;
    input.select();
    input.setSelectionRange(0, input.value.length);
    let copied = false;
    try { copied = document.execCommand('copy'); } catch (e) {}
    button.textContent = copied ? 'Copied' : 'Selected';
    setTimeout(() => { button.textContent = 'Copy'; }, 1500);
  };
}

// The copied "My API Application" page fills both fields. A single value is pasted as usual.
function pasteApp(event) {
  const text = (event.clipboardData || window.clipboardData).getData('text');
  const id = text.match(/Client ID\D*?(\d+)/i);
  const secret = text.match(/Client Secret\W*?([0-9a-f]{40})\b/i);
  if (!id && !secret) return;
  event.preventDefault();
  if (id) document.getElementById('clientId').value = id[1];
  if (secret) document.getElementById('clientSecret').value = secret[1];
}
document.getElementById('clientId').addEventListener('paste', pasteApp);
document.getElementById('clientSecret').addEventListener('paste', pasteApp);
</script>
</body></html>
)html";

// %UPDATE% if a newer release was found, in the normal WiFi: the download needs internet
static const char UPDATE_FORM[] = R"html(
<h2>Firmware update</h2>
<p>Version <b>%NEW_VERSION%</b> is available.
<a href="https://github.com/joj0x90/ePaper-strava-viewer/releases/tag/%NEW_VERSION%" target="_blank" rel="noopener">What's new</a></p>
<form method="post" action="/update" onsubmit="return confirm('Install %NEW_VERSION% now? The device restarts afterwards.')">
<button type="submit">Install update</button>
</form>
)html";

// %UPDATE% if a newer release was found, in the device's own WiFi
static const char UPDATE_OFFLINE[] = R"html(
<h2>Firmware update</h2>
<p class="note">Version <b>%NEW_VERSION%</b> is available. Installing it needs internet: save
the WiFi first, then open this page in your WiFi with the button on the device.</p>
)html";

// %STRAVA_CONNECT% in the normal WiFi: authorizing needs internet. Both buttons first store the
// Client ID and secret from the form, the code exchange needs them.
static const char STRAVA_CONNECT[] = R"html(
<button type="submit" formaction="/strava/connect">Connect with Strava</button>
<small>Opens Strava's authorization page. After you authorize, Strava leads you back here.</small>
<details><summary>Strava doesn't lead you back here?</summary>
<p><small>If the browser shows an error page after authorizing (some phones can't open
.local addresses), copy that page's full address from the address bar and paste it here.
This also works with <b>localhost</b> as Authorization Callback Domain: authorize with the
second button below.</small></p>
<label for="redirect">Address of the error page</label>
<input id="redirect" name="redirect" placeholder="http://...?state=...&amp;code=..." autocomplete="off">
<button type="submit" formaction="/strava/code">Connect with this address</button>
<button type="submit" formaction="/strava/connect" name="via" value="localhost" class="secondary">Authorize via localhost</button>
</details>
)html";

// %STRAVA_CONNECT% in the device's own WiFi
static const char STRAVA_OFFLINE[] = R"html(
<p class="note">Connecting to Strava needs internet. Save the WiFi first: the device then opens
this page in your WiFi, with a button to connect Strava.</p>
)html";

// Answer after saving or resetting, as the device stops the page right afterwards
static const char ANSWER_PAGE[] = R"html(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Strava Viewer setup</title>
<style>body{font-family:system-ui,sans-serif;max-width:32rem;margin:0 auto;padding:1rem}</style>
</head><body><h1>%TITLE%</h1><p>%TEXT%</p></body></html>
)html";

static WebServer server(80);
static DNSServer dnsServer;

static bool usingAccessPoint = false;
static DeviceStatus deviceStatus;
static uint32_t startMs = 0, durationMs = 0;
static String networkOptions;  // <option> per WiFi network found, for the network name field
static bool finished = false;
static SetupResult result = SetupResult::TimedOut;
// Random per setup mode and sent to Strava, which hands it back: a code that comes with another
// state wasn't requested by this page
static String oauthState;
// Shown once at the top of the next page, e.g. after connecting Strava
static String banner;
static bool bannerIsError = false;

static String htmlEscape(const String &text) {
    String out;
    out.reserve(text.length());
    for (char c : text) {
        switch (c) {
            case '&':  out += "&amp;"; break;
            case '<':  out += "&lt;"; break;
            case '>':  out += "&gt;"; break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default:   out += c;
        }
    }
    return out;
}

static uint32_t remainingSeconds() {
    const uint32_t elapsed = millis() - startMs;
    return elapsed >= durationMs ? 0 : (durationMs - elapsed) / 1000;
}

// Lists the networks in range once, before the access point starts: scanning while it runs
// would interrupt it.
static void scanNetworks() {
    networkOptions = "";
    const int16_t count = WiFi.scanNetworks();
    String listed = "\n";  // names already listed, each followed by '\n'
    for (int16_t i = 0, shown = 0; i < count && shown < MAX_LISTED_NETWORKS; i++) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty() || listed.indexOf("\n" + ssid + "\n") >= 0) continue;  // hidden, dup
        listed += ssid + "\n";
        networkOptions += "<option value=\"" + htmlEscape(ssid) + "\">";
        shown++;
    }
    WiFi.scanDelete();
}

static String urlEncode(const String &text) {
    String out;
    char hex[4];
    for (char c : text) {
        if (isalnum((unsigned char)c) || strchr("-_.~", c)) {
            out += c;
        } else {
            snprintf(hex, sizeof(hex), "%%%02X", (uint8_t)c);
            out += hex;
        }
    }
    return out;
}

static String urlDecode(const String &text) {
    String out;
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] == '%' && i + 2 < text.length()) {
            out += (char)strtol(text.substring(i + 1, i + 3).c_str(), nullptr, 16);
            i += 2;
        } else {
            out += text[i] == '+' ? ' ' : text[i];
        }
    }
    return out;
}

// Value of the query parameter name in a pasted URL, or "" if it has none
static String queryParam(const String &url, const char *name) {
    const String key = String(name) + "=";
    int start = url.indexOf("?" + key);
    if (start < 0) start = url.indexOf("&" + key);
    if (start < 0) return "";
    start += key.length() + 1;
    int end = url.indexOf('&', start);
    if (end < 0) end = url.indexOf('#', start);
    return urlDecode(url.substring(start, end < 0 ? url.length() : end));
}

static void setBanner(const String &text, bool error) {
    banner = text;
    bannerIsError = error;
}

static void redirectHome() {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
}

static String storedHint(const String &value) {
    return value.isEmpty() ? "not set" : "stored, leave empty to keep";
}

static void sendAnswer(const char *title, const String &text) {
    String page = ANSWER_PAGE;
    page.replace("%TITLE%", title);
    page.replace("%TEXT%", text);
    server.send(200, "text/html", page);
}

static void handleRoot() {
    const Credentials c = loadCredentials();

    String battery = "no battery connected";
    if (deviceStatus.batteryPercent >= 0) {
        battery = String(deviceStatus.batteryPercent) + " % (" +
                  String(deviceStatus.batteryVolts, 2) + " V)";
    }
    String connection;
    if (!usingAccessPoint) {
        connection =
            "Connected to " + htmlEscape(c.wifiSsid) + " (" + WiFi.localIP().toString() + ")";
    } else if (c.wifiSsid.isEmpty()) {
        connection = "Own WiFi " + htmlEscape(accessPointSsid()) + ", not set up yet";
    } else {
        connection = "Own WiFi " + htmlEscape(accessPointSsid()) + ", couldn't connect to " +
                     htmlEscape(c.wifiSsid);
    }
    const bool stravaComplete = !c.stravaClientId.isEmpty() && !c.stravaClientSecret.isEmpty() &&
                                !c.stravaRefreshToken.isEmpty();

    String page = SETTINGS_PAGE;
    page.replace("%MINUTES%", String((remainingSeconds() + 59) / 60));
    page.replace("%SECONDS%", String(remainingSeconds()));
    page.replace("%FIRMWARE%", htmlEscape(FIRMWARE_VERSION));
    page.replace("%BATTERY%", battery);
    page.replace("%CONNECTION%", connection);
    page.replace("%STRAVA%", stravaComplete ? "credentials stored" : "credentials incomplete");
    page.replace("%SSID%", htmlEscape(c.wifiSsid));
    page.replace("%NETWORKS%", networkOptions);
    page.replace("%PASSWORD_HINT%", c.wifiPassword.isEmpty() ? "" : storedHint(c.wifiPassword));
    page.replace("%CLIENT_ID%", htmlEscape(c.stravaClientId));
    page.replace("%GUIDE_OPEN%", c.stravaClientId.isEmpty() ? " open" : "");
    page.replace("%SECRET_HINT%", storedHint(c.stravaClientSecret));
    page.replace("%TOKEN_HINT%", storedHint(c.stravaRefreshToken));
    page.replace("%STRAVA_CONNECT%", usingAccessPoint ? STRAVA_OFFLINE : STRAVA_CONNECT);
    const String update = availableUpdate();
    if (update.isEmpty()) {
        page.replace("%UPDATE%", "");
    } else {
        page.replace("%UPDATE%", usingAccessPoint ? UPDATE_OFFLINE : UPDATE_FORM);
        page.replace("%NEW_VERSION%", htmlEscape(update));
    }
    page.replace("%BANNER%", banner.isEmpty()
                                 ? String()
                                 : String("<p class=\"banner ") + (bannerIsError ? "error" : "ok") +
                                       "\">" + htmlEscape(banner) + "</p>");
    banner = "";
    server.send(200, "text/html", page);
}

// Trimmed form field; copied tokens often come with spaces or a line break
static String formField(const char *name) {
    String value = server.arg(name);
    value.trim();
    return value;
}

static void handleSave() {
    Credentials c = loadCredentials();
    const String ssid = formField("ssid");
    const String password = server.arg("password");  // spaces can be part of a password
    if (ssid.isEmpty() || ssid.length() > 32) {
        server.send(400, "text/plain", "The WiFi name must have 1 to 32 characters.");
        return;
    }
    if (!password.isEmpty() && (password.length() < 8 || password.length() > 63)) {
        server.send(400, "text/plain", "A WiFi password has 8 to 63 characters.");
        return;
    }

    c.wifiSsid = ssid;
    if (!password.isEmpty()) c.wifiPassword = password;
    c.stravaClientId = formField("clientId");
    // empty fields keep the stored secrets, which the page never sends to the browser
    if (!formField("clientSecret").isEmpty()) c.stravaClientSecret = formField("clientSecret");
    if (!formField("refreshToken").isEmpty()) c.stravaRefreshToken = formField("refreshToken");
    saveCredentials(c);
    Serial.printf("Setup: saved the settings for WiFi '%s'\n", c.wifiSsid.c_str());

    sendAnswer("Saved", "The device now connects to " + htmlEscape(c.wifiSsid) +
                            " and shows your latest activity. You can close this page.");
    result = SetupResult::Saved;
    finished = true;
}

// Stores the Client ID and secret from the form, which the code exchange needs. Returns false
// (with a banner) if one of them is missing.
static bool saveStravaApp() {
    Credentials c = loadCredentials();
    c.stravaClientId = formField("clientId");
    if (!formField("clientSecret").isEmpty()) c.stravaClientSecret = formField("clientSecret");
    if (c.stravaClientId.isEmpty() || c.stravaClientSecret.isEmpty()) {
        setBanner("Enter the Client ID and Client Secret of your Strava app first.", true);
        return false;
    }
    saveCredentials(c);
    return true;
}

// Opens Strava's authorization page, which leads back to /strava/callback, or with via=localhost
// to an error page whose address is pasted into the setup page
static void handleStravaConnect() {
    if (!saveStravaApp()) return redirectHome();
    const String redirectUri =
        server.arg("via") == "localhost" ? STRAVA_LOCALHOST_URL : STRAVA_CALLBACK_URL;
    const String url =
        "https://www.strava.com/oauth/authorize?client_id=" +
        urlEncode(loadCredentials().stravaClientId) +
        "&response_type=code&approval_prompt=force&scope=" + urlEncode(STRAVA_SCOPE) +
        "&state=" + oauthState + "&redirect_uri=" + urlEncode(redirectUri);
    server.sendHeader("Location", url);
    server.send(303, "text/plain", "");
}

// Exchanges the code from Strava's authorization page for the refresh token and stores it.
// error, state and scope are the other parameters Strava sent along (empty if not pasted).
static void connectStrava(const String &code, const String &error, const String &state,
                          const String &scope) {
    if (!error.isEmpty()) {
        setBanner("Strava wasn't authorized (" + error + ").", true);
        return;
    }
    if (code.isEmpty()) {
        setBanner("There's no code in this address. Paste the full address after authorizing.",
                  true);
        return;
    }
    if (!state.isEmpty() && state != oauthState) {
        setBanner("This code belongs to an older attempt. Connect with Strava again.", true);
        return;
    }
    Serial.printf("Setup: Strava granted '%s'\n", scope.isEmpty() ? "(unknown)" : scope.c_str());
    if (!scope.isEmpty() && scope.indexOf("activity:read") < 0) {
        setBanner("Allow Strava to show your activities (keep that box checked) and connect again.",
                  true);
        return;
    }

    Credentials c = loadCredentials();
    String refreshToken, athleteName;
    const StravaError stravaError =
        exchangeAuthorizationCode(c.stravaClientId.c_str(), c.stravaClientSecret.c_str(),
                                  code.c_str(), refreshToken, athleteName);
    if (!stravaError.ok()) {
        Serial.printf("Setup: Strava code exchange failed (HTTP %d): %s\n", stravaError.httpStatus,
                      stravaError.detail.c_str());
        setBanner(String(stravaError.message()) + " (HTTP " + stravaError.httpStatus +
                      "). Check the Client ID and Client Secret, then connect again.",
                  true);
        return;
    }
    c.stravaRefreshToken = refreshToken;
    saveCredentials(c);
    Serial.printf("Setup: connected to Strava as %s\n", athleteName.c_str());
    setBanner("Strava is connected (" + athleteName + "). Save to show your latest activity.",
              false);
}

// Strava's authorization page leads here (callback domain strava-viewer.local)
static void handleStravaCallback() {
    connectStrava(server.arg("code"), server.arg("error"), server.arg("state"),
                  server.arg("scope"));
    redirectHome();  // so reloading the page doesn't send the used code again
}

// The pasted address of the page Strava led to, if that didn't reach the device
static void handleStravaCode() {
    if (!saveStravaApp()) return redirectHome();
    const String pasted = formField("redirect");
    // a bare code works too
    const String code = pasted.indexOf('=') < 0 ? pasted : queryParam(pasted, "code");
    connectStrava(code, queryParam(pasted, "error"), queryParam(pasted, "state"),
                  queryParam(pasted, "scope"));
    redirectHome();
}

static void handleReset() {
    Serial.println("Setup: resetting the device");
    sendAnswer("Device reset",
               "All settings are deleted. The device restarts and opens its own WiFi for the "
               "setup again, with a new password shown on the display.");
    result = SetupResult::Reset;
    finished = true;
}

static void handleUpdate() {
    const String update = availableUpdate();
    if (usingAccessPoint || update.isEmpty()) return redirectHome();  // the page offered none
    Serial.printf("Setup: installing %s\n", update.c_str());
    sendAnswer("Installing the update",
               "The device downloads " + htmlEscape(update) +
                   " and restarts with it, this can take a minute. The display shows the progress. "
                   "You can close this page.");
    result = SetupResult::Update;
    finished = true;
}

// The icon to upload when creating the Strava API application (tools/app_icon/make_icon.py)
static void handleAppIcon() {
    server.sendHeader("Content-Disposition", "attachment; filename=\"strava-viewer-icon.png\"");
    server.send_P(200, "image/png", reinterpret_cast<const char *>(APP_ICON_PNG),
                  sizeof(APP_ICON_PNG));
}

// In the access point every address leads to the setup page: phones check a known address after
// connecting and open the captive portal when they get redirected.
static void handleNotFound() {
    if (!usingAccessPoint) {
        server.send(404, "text/plain", "Not found");
        return;
    }
    server.sendHeader("Location", SETUP_AP_URL);
    server.send(302, "text/plain", "");
}

String startSetupPortal(bool accessPoint, const DeviceStatus &status, uint32_t duration) {
    usingAccessPoint = accessPoint;
    deviceStatus = status;
    durationMs = duration;
    finished = false;
    result = SetupResult::TimedOut;
    oauthState = String(esp_random(), HEX) + String(esp_random(), HEX);
    banner = "";

    if (accessPoint) {
        // stop retrying a WiFi that failed: its channel hopping would disturb the access point
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP_STA);  // the station part is needed for scanning
    }
    scanNetworks();

    String url;
    if (accessPoint) {
        WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
        WiFi.softAP(accessPointSsid().c_str(), accessPointPassword().c_str());
        dnsServer.start(53, "*", AP_IP);  // answers every name with the device's address
        url = SETUP_AP_URL;
    } else {
        if (MDNS.begin(MDNS_HOSTNAME)) MDNS.addService("http", "tcp", 80);
        url = "http://" + WiFi.localIP().toString() + "/";
    }

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/app-icon.png", HTTP_GET, handleAppIcon);
    server.on("/update", HTTP_POST, handleUpdate);
    server.on("/strava/connect", HTTP_POST, handleStravaConnect);
    server.on("/strava/callback", HTTP_GET, handleStravaCallback);
    server.on("/strava/code", HTTP_POST, handleStravaCode);
    server.onNotFound(handleNotFound);
    server.begin();
    startMs = millis();
    return url;
}

SetupResult runSetupPortal() {
    while (!finished && remainingSeconds() > 0) {
        if (usingAccessPoint) dnsServer.processNextRequest();
        server.handleClient();
        delay(2);
    }
    if (finished) {
        const uint32_t answeredMs = millis();
        while (millis() - answeredMs < ANSWER_DELAY_MS) {
            server.handleClient();
            delay(2);
        }
    } else {
        Serial.println("Setup: time is up");
    }

    server.stop();
    if (usingAccessPoint) {
        dnsServer.stop();
    } else {
        MDNS.end();
    }
    WiFi.mode(WIFI_OFF);
    if (result == SetupResult::Reset) eraseAllSettings();
    return result;
}
