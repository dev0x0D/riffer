#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <vector>

// Decodes anything ffmpeg can read into 48 kHz stereo f32 in memory, via
// the ffmpeg command line (the Omarchy convention, rather than linking
// libav). The executables are injectable so bin/test can run fake ones.
class Decoder : public QObject {
    Q_OBJECT

public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;

    explicit Decoder(const QString &ffmpeg = {}, const QString &ffprobe = {},
                     QObject *parent = nullptr);
    ~Decoder() override;

    bool busy() const { return m_state == State::Decoding; }
    void load(const QString &path);
    void cancel();
    std::vector<float> takeFrames() { return std::move(m_frames); }

    // Duration in seconds from ffprobe, or a negative number on failure.
    // Only used for the progress bar; the decoded signal carries the real
    // length computed from the samples.
    double probeDurationBlocking(const QString &path) const;

signals:
    void progressed(double fraction);
    void decoded(const QString &path, double seconds);
    void failed(const QString &message);

private:
    enum class State { Idle, Decoding };

    void fail(const QString &message);

    QString m_ffmpeg;
    QString m_ffprobe;
    State m_state = State::Idle;
    QProcess m_process;
    std::vector<float> m_frames;
    QByteArray m_partial; // stdout bytes not yet a whole 4-byte sample
    QString m_path;
    double m_duration = 0;
    int m_lastPercent = -1;
};
