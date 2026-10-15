#pragma once
// Minimal Arduino stand-ins so Adafruit GFX compiles on the PC (icon preview only).
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
using std::min; using std::max;
typedef bool boolean;
#define PROGMEM
class __FlashStringHelper;
class String {
public:
  String(const char *s = "") : s_(s) {}
  const char *c_str() const { return s_; }
  unsigned length() const { return strlen(s_); }
private:
  const char *s_;
};
#define radians(d) ((d) * M_PI / 180.0)
