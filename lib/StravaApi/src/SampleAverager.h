#ifndef SAMPLE_AVERAGER_H
#define SAMPLE_AVERAGER_H

#include <stdint.h>

// Averages a series of samples of unknown length (e.g. read straight from a download) into a
// few equal parts, in constant memory (8 KB): the samples go into up to MAX_BUCKETS buckets of
// samplesPerBucket samples each; when they run out, neighbouring buckets are merged and each
// bucket takes twice as many samples from then on. In the end, every part gets its share of the
// buckets, so with more than MAX_BUCKETS / 2 buckets the parts differ in length by at most one
// bucket. Samples of 0 count for the length, but not for the average (sensor dropouts).
class SampleAverager {
public:
    void add(uint16_t value) {
        if (bucketCount_ == 0 || bucket_[bucketCount_ - 1].samples == samplesPerBucket_) {
            if (bucketCount_ == MAX_BUCKETS) mergePairs();
            bucket_[bucketCount_++] = Bucket();
        }
        Bucket &b = bucket_[bucketCount_ - 1];
        b.samples++;
        if (value > 0) {
            b.sum += value;
            b.valid++;
        }
        sampleCount_++;
    }

    // Starts a new series, without the 8 KB temporary that assigning a new object would need
    void reset() {
        bucketCount_ = 0;
        samplesPerBucket_ = 1;
        sampleCount_ = 0;
    }

    uint32_t sampleCount() const { return sampleCount_; }

    // Writes the rounded average of at most maxParts equal parts (fewer if there are fewer
    // samples), 0 for parts without a valid sample. Returns the number of parts written.
    uint8_t averages(uint8_t *out, uint8_t maxParts) const {
        const uint32_t parts = sampleCount_ < maxParts ? sampleCount_ : maxParts;
        for (uint32_t part = 0; part < parts; part++) {
            const uint32_t first = part * bucketCount_ / parts;
            const uint32_t end = (part + 1) * bucketCount_ / parts;
            uint32_t sum = 0, valid = 0;
            for (uint32_t i = first; i < end; i++) {
                sum += bucket_[i].sum;
                valid += bucket_[i].valid;
            }
            const uint32_t avg = valid > 0 ? (sum + valid / 2) / valid : 0;
            out[part] = avg > 255 ? 255 : avg;
        }
        return parts;
    }

private:
    static const uint16_t MAX_BUCKETS = 1024;

    struct Bucket {
        uint32_t sum = 0;
        uint16_t valid = 0;    // samples > 0
        uint16_t samples = 0;  // all samples
    };

    // Halves the number of buckets; all of them are full when this is called.
    void mergePairs() {
        for (uint16_t i = 0; i < MAX_BUCKETS / 2; i++) {
            const Bucket &a = bucket_[2 * i];
            const Bucket &b = bucket_[2 * i + 1];
            Bucket merged;
            merged.sum = a.sum + b.sum;
            merged.valid = a.valid + b.valid;
            merged.samples = a.samples + b.samples;
            bucket_[i] = merged;
        }
        bucketCount_ = MAX_BUCKETS / 2;
        samplesPerBucket_ *= 2;
    }

    Bucket bucket_[MAX_BUCKETS];
    uint16_t bucketCount_ = 0;
    uint16_t samplesPerBucket_ = 1;
    uint32_t sampleCount_ = 0;
};

#endif  // SAMPLE_AVERAGER_H
