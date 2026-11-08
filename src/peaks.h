#pragma once

#include <QtGlobal>
#include <vector>

// One min/max pair over a run of frames of the mono mix.
struct Peak {
    float minV = 0.0f;
    float maxV = 0.0f;
};

// A pyramid level: peaks[i] covers frames [i*bucketFrames, (i+1)*bucketFrames).
struct PeakLevel {
    qint64 bucketFrames = 0;
    std::vector<Peak> peaks;
};

// Builds a min/max pyramid over interleaved audio so any zoom level renders
// from O(pixels) buckets instead of walking every sample. Level 0 aggregates
// 256 frames per bucket (deep zoom reads the raw samples instead); each level
// above aggregates 256 buckets of the level below.
std::vector<PeakLevel> buildPeaks(const float *frames, qint64 frameCount, int channels);

// Aggregates buckets [firstBucket, firstBucket+count) of a level into one pair.
Peak combinePeaks(const PeakLevel &level, qint64 firstBucket, qint64 count);

// The deepest level whose buckets are no coarser than wanted, or -1.
int levelForBucket(const std::vector<PeakLevel> &levels, qint64 bucketFrames);
