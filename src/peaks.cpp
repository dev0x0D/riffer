#include "peaks.h"

#include <algorithm>

std::vector<PeakLevel> buildPeaks(const float *frames, qint64 frameCount, int channels) {
    std::vector<PeakLevel> levels;
    if (!frames || frameCount <= 0 || channels <= 0)
        return levels;

    auto mono = [&](qint64 f) {
        return channels == 1 ? frames[f] : (frames[f * 2] + frames[f * 2 + 1]) / 2.0f;
    };

    const qint64 base = 256;
    PeakLevel level;
    level.bucketFrames = base;
    const qint64 buckets = (frameCount + base - 1) / base;
    level.peaks.resize(size_t(buckets));
    for (qint64 b = 0; b < buckets; ++b) {
        float lo = 1.0f, hi = -1.0f;
        for (qint64 f = b * base, end = std::min((b + 1) * base, frameCount); f < end; ++f) {
            const float v = mono(f);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        level.peaks[size_t(b)] = Peak{lo, hi};
    }
    levels.push_back(std::move(level));

    while (levels.back().peaks.size() > 1) {
        const PeakLevel &previous = levels.back();
        PeakLevel next;
        next.bucketFrames = previous.bucketFrames * 256;
        const size_t count = (previous.peaks.size() + 255) / 256;
        next.peaks.resize(count);
        for (size_t i = 0; i < count; ++i) {
            const size_t begin = i * 256;
            const size_t end = std::min(begin + size_t(256), previous.peaks.size());
            Peak out{previous.peaks[begin].minV, previous.peaks[begin].maxV};
            for (size_t k = begin + 1; k < end; ++k) {
                out.minV = std::min(out.minV, previous.peaks[k].minV);
                out.maxV = std::max(out.maxV, previous.peaks[k].maxV);
            }
            next.peaks[i] = out;
        }
        levels.push_back(std::move(next));
    }
    return levels;
}

Peak combinePeaks(const PeakLevel &level, qint64 firstBucket, qint64 count) {
    if (level.peaks.empty())
        return Peak{};
    firstBucket = std::max<qint64>(0, firstBucket);
    count = std::max<qint64>(0, std::min(count, qint64(level.peaks.size()) - firstBucket));
    if (count <= 0)
        return Peak{};
    Peak out = level.peaks[size_t(firstBucket)];
    for (qint64 i = 1; i < count; ++i) {
        const Peak &p = level.peaks[size_t(firstBucket + i)];
        out.minV = std::min(out.minV, p.minV);
        out.maxV = std::max(out.maxV, p.maxV);
    }
    return out;
}

int levelForBucket(const std::vector<PeakLevel> &levels, qint64 bucketFrames) {
    int best = -1;
    for (size_t i = 0; i < levels.size(); ++i)
        if (levels[i].bucketFrames <= bucketFrames)
            best = int(i);
    return best;
}
