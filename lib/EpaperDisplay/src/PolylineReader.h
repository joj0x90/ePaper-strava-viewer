#ifndef POLYLINE_READER_H
#define POLYLINE_READER_H

#include <stdint.h>

// Reads the points of an encoded polyline (Google's format, as in Strava's summary_polyline)
// one at a time, without decoding the whole line into memory.
class PolylineReader {
public:
    // The string must stay valid while reading
    explicit PolylineReader(const char *encoded) : p_(encoded) {}

    // Next point in 1e-5 degrees; false at the end (or if the string is malformed)
    bool next(int32_t &lat, int32_t &lng);

private:
    bool readValue(int32_t &value);

    const char *p_;
    int32_t lat_ = 0;
    int32_t lng_ = 0;
};

#endif  // POLYLINE_READER_H
