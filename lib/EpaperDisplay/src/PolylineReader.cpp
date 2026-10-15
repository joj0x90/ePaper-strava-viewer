#include "PolylineReader.h"

bool PolylineReader::next(int32_t &lat, int32_t &lng) {
    int32_t dLat, dLng;
    if (!readValue(dLat) || !readValue(dLng)) return false;
    lat_ += dLat;
    lng_ += dLng;
    lat = lat_;
    lng = lng_;
    return true;
}

// Each point is stored as the difference to the previous one, as a variable-length
// number: 5-bit chunks, lowest first, offset by 63 to be printable ASCII.
bool PolylineReader::readValue(int32_t &value) {
    uint32_t result = 0;
    uint8_t shift = 0;
    int32_t chunk;
    do {
        if (*p_ < 63 || shift > 30) return false;  // end of string or invalid character
        chunk = *p_++ - 63;
        result |= (uint32_t)(chunk & 0x1F) << shift;
        shift += 5;
    } while (chunk >= 0x20);
    value = (result & 1) ? ~(int32_t)(result >> 1) : (int32_t)(result >> 1);
    return true;
}
