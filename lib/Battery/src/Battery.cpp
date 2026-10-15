#include "Battery.h"

// ADC1 pin: ADC2 can't be used while WiFi is on
static const uint8_t BATTERY_PIN = 35;
// R1 = R2 = 100 kΩ halve the battery voltage
static const float DIVIDER_RATIO = 2.0f;
// Averaging smooths out the ADC noise
static const uint8_t SAMPLES = 16;
// Below this, the pin only sees the divider pulling it to GND: no battery connected
static const float NO_BATTERY_V = 2.5f;

// Resting voltage of a Li-Ion cell by charge, highest first. 0 % is where the HT7333 can no
// longer keep 3.3 V and the ESP32 is about to brown out, not where the DW01 cuts off (2.5 V).
struct ChargePoint {
    float volts;
    uint8_t percent;
};
static const ChargePoint CHARGE_CURVE[] = {
    {4.20f, 100}, {4.10f, 90}, {4.00f, 80}, {3.92f, 70}, {3.86f, 60}, {3.82f, 50},
    {3.79f, 40},  {3.76f, 30}, {3.73f, 20}, {3.69f, 10}, {3.60f, 5},  {3.30f, 0},
};

float readBatteryVoltage() {
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);  // measures up to ~2.5 V accurately
    uint32_t sumMv = 0;
    for (uint8_t i = 0; i < SAMPLES; i++) {
        sumMv += analogReadMilliVolts(BATTERY_PIN);  // corrected by the eFuse calibration
    }
    return sumMv / SAMPLES / 1000.0f * DIVIDER_RATIO;
}

int8_t batteryPercent(float volts) {
    if (volts < NO_BATTERY_V) return -1;

    const size_t count = sizeof(CHARGE_CURVE) / sizeof(CHARGE_CURVE[0]);
    if (volts >= CHARGE_CURVE[0].volts) return 100;
    for (size_t i = 1; i < count; i++) {
        const ChargePoint &hi = CHARGE_CURVE[i - 1];
        const ChargePoint &lo = CHARGE_CURVE[i];
        if (volts >= lo.volts) {  // interpolate linearly between the two points
            return lo.percent +
                   (volts - lo.volts) / (hi.volts - lo.volts) * (hi.percent - lo.percent) + 0.5f;
        }
    }
    return 0;
}
