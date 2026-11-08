#include "clock.h"

void PlayClock::reset(double inputFrame, double tempo) {
    m_points.clear();
    m_points.push_back(Point{0, inputFrame, tempo});
}

void PlayClock::push(const Point &point) {
    // A slider drag fires many changes in one event loop turn; one output
    // frame can only ever have one mapping, so replace instead of append.
    if (!m_points.empty() && m_points.back().out == point.out) {
        m_points.back() = point;
        return;
    }
    if (!m_points.empty() && m_points.back().out > point.out)
        return; // Points must advance; a late record is dropped.
    m_points.push_back(point);
    // Only the recent past is ever queried, but a long practice session with
    // a loop every few seconds would otherwise grow this without bound.
    if (m_points.size() > 512)
        m_points.erase(m_points.begin(), m_points.end() - 256);
}

void PlayClock::setTempo(qint64 outFrames, double tempo) {
    if (m_points.empty()) {
        reset(0, tempo);
        return;
    }
    // Up to this output frame the old mapping holds; from here the new one.
    push(Point{outFrames, inputAt(outFrames), tempo});
}

void PlayClock::wrapped(qint64 outFrames, double inputFrame) {
    const double tempo = m_points.empty() ? 1.0 : m_points.back().tempo;
    push(Point{outFrames, inputFrame, tempo});
}

double PlayClock::inputAt(qint64 outFrames) const {
    if (m_points.empty())
        return 0;
    // The last point at or before outFrames.
    int lo = 0, hi = int(m_points.size()) - 1, idx = 0;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (m_points[mid].out <= outFrames) {
            idx = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    const Point &p = m_points[idx];
    return p.inFrame + double(outFrames - p.out) * p.tempo;
}
