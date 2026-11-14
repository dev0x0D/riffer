#pragma once

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <vector>

class QAudioSink;
class QIODevice;

// A click track on its own audio output: steady pulses at a fixed BPM with an
// optional accent on the first beat of the bar. Independent of the Player, so
// it clicks along with a track or alone for exercises; the system mixer
// combines the two sinks. Scheduling is sample-accurate: the feeder builds
// each chunk of silence and mixes pre-synthesized click buffers at the exact
// frames beats land on — the same push-mode pattern the Player uses (Qt
// 6.12's ffmpeg backend never pulls from a custom QIODevice).
class Metronome : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int bpm READ bpm WRITE setBpm NOTIFY bpmChanged)
    // Beats per bar: 1 = plain ticks, 2..7 accent the first beat of the bar.
    Q_PROPERTY(int beatsPerBar READ beatsPerBar WRITE setBeatsPerBar NOTIFY beatsPerBarChanged)
    // One of the synthesized clicks, 0..kSoundCount-1.
    Q_PROPERTY(int sound READ sound WRITE setSound NOTIFY soundChanged)
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY gainChanged)

public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;
    static constexpr int kSoundCount = 4; // wood, click, beep, block

    explicit Metronome(QObject *parent = nullptr);
    ~Metronome() override;

    bool enabled() const { return m_enabled; }
    int bpm() const { return m_bpm; }
    int beatsPerBar() const { return m_beatsPerBar; }
    int sound() const { return m_sound; }
    double gain() const { return m_gain; }

    // Synthesized beat at 48 kHz mono, peak-normalized to 1.0; the pump
    // scales it by the gain and the accent weight. Public for tests.
    QVector<float> clickSamples(int sound, bool accent) const;
    // tap() with an explicit timestamp, so tests can feed a rhythm instantly.
    qint64 tapAt(qint64 ms);

public slots:
    void setEnabled(bool enabled);
    void setBpm(int bpm);           // clamped to 30..300
    void setBeatsPerBar(int beats); // clamped to 1..7
    void setSound(int sound);       // clamped to 0..kSoundCount-1
    void setGain(double gain);      // clamped to 0..1
    Q_INVOKABLE void tap();         // tap tempo: recent clicks set the BPM
    void restart();                 // back to beat one, e.g. when playback starts

signals:
    void enabledChanged();
    void bpmChanged();
    void beatsPerBarChanged();
    void soundChanged();
    void gainChanged();
    void errorOccurred(const QString &message);

private:
    void rebuildClicks();
    bool startSink();
    void pump(); // feeder tick: generate silence + clicks up to the sink's free space

    bool m_enabled = false;
    int m_bpm = 120;
    int m_beatsPerBar = 4;
    int m_sound = 0;
    double m_gain = 0.8;

    QVector<float> m_click;  // plain beat, mono, peak 1.0
    QVector<float> m_accent; // first-of-bar beat, brighter, peak 1.0
    QVector<float> m_carry;  // tail of a click that crossed the chunk border
    QAudioSink *m_sink = nullptr;
    QIODevice *m_sinkDevice = nullptr; // the sink's push-mode device
    QTimer m_feeder;
    QAudioFormat m_format;
    QElapsedTimer m_tapClock;
    QVector<qint64> m_taps;   // recent tap timestamps in ms
    qint64 m_framesWritten = 0;  // output frames handed to the sink so far
    qint64 m_nextClickFrame = 0; // absolute output frame the next beat starts at
    qint64 m_beatIndex = 0;
    std::vector<float> m_scratch; // stereo chunk under construction
};
