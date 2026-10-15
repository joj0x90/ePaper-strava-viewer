#pragma once
// Minimal Arduino stand-ins so Adafruit GFX compiles on the PC (icon preview only).
#include <stddef.h>
#include <stdint.h>
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t) = 0;
  size_t print(const char *s) { size_t c = 0; while (*s) c += write((uint8_t)*s++); return c; }
  virtual size_t write(const uint8_t *b, size_t n) { size_t c = 0; while (n--) c += write(*b++); return c; }
};
