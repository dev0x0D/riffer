#pragma once

#include <QtGlobal>
#include <vector>

// Maps output (played) frames back onto the input timeline across tempo
// changes and loop restarts. Positions are input frames held as double
// because a tempo maps them fractionally. Points must be recorded for the
// moment a change is *heard*, not when it is fed into SoundTouch, so the
// caller shifts by the pipeline latency before calling setTempo() or
// wrapped(). Pure logic: no audio device, so bin/test covers it.
class PlayClock {
public:
    void reset(double inputFrame, double tempo);
    void setTempo(qint64 outFrames, double tempo);
    void wrapped(qint64 outFrames, double inputFrame);
    double inputAt(qint64 outFrames) const;

private:
    struct Point {
        qint64 out;
        double inFrame;
        double tempo;
    };

    void push(const Point &point);

    std::vector<Point> m_points;
};
