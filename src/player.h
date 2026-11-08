#pragma once

#include <QAudioFormat>
#include <QObject>

#include "clock.h"

class QAudioSink;
namespace soundtouch {
class SoundTouch;
}

// The practice engine: plays an in-memory stereo f32 buffer through
// SoundTouch (speed changes keep the pitch) into QAudioSink, looping the
// active range gaplessly — the wrap happens inside the stretch pipeline, so
// there is no seam to hear. All timing math lives in PlayClock so it stays
// testable without an audio device.
class Player : public QObject {
    Q_OBJECT

public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    void setSource(const float *frames, qint64 frameCount);

    bool playing() const { return m_playing; }
    double speed() const { return m_speed; }
    double volume() const { return m_volume; }
    double loopStart() const { return m_loopStartF / double(kSampleRate); }
    double loopEnd() const { return m_loopEndF / double(kSampleRate); }
    double inputPosition() const; // seconds; the loop start when stopped

public slots:
    void setLoop(double startSec, double endSec);
    void setSpeed(double speed);  // 0.2 .. 1.5, pitch preserved
    void setVolume(double volume); // 0 .. 1
    void play();                  // always restarts from the loop start
    void stop();
    void seek(double sec);        // live jump while playing

signals:
    void playingChanged();
    void speedChanged(double speed);
    void volumeChanged(double volume);
    void errorOccurred(const QString &message);

private:
    class Stream; // QIODevice pulling from SoundTouch for QAudioSink
    friend class Stream;

    qint64 frameOf(double sec) const {
        return qBound<qint64>(0, qRound64(sec * kSampleRate), m_frameCount);
    }
    qint64 latencyOutFrames() const;
    void resetPipeline(qint64 startFrame);
    void fill(qint64 framesWanted);

    const float *m_frames = nullptr;
    qint64 m_frameCount = 0;
    qint64 m_loopStartF = 0;
    qint64 m_loopEndF = 0;
    qint64 m_nextInput = 0;
    qint64 m_outFrames = 0; // output frames handed to the device so far
    double m_speed = 1.0;
    double m_volume = 1.0;
    bool m_playing = false;
    soundtouch::SoundTouch *m_st = nullptr;
    QAudioSink *m_sink = nullptr;
    Stream *m_stream = nullptr;
    QAudioFormat m_format;
    PlayClock m_clock;
};
