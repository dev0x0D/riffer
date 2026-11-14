#include "metronome.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QRandomGenerator>
#include <cmath>

namespace {
// Feed the sink roughly every 20 ms; its buffer (~100 ms) absorbs jitter.
constexpr int kFeederMs = 20;
constexpr int kMinBpm = 30;
constexpr int kMaxBpm = 300;
constexpr int kMaxBeats = 7;
constexpr int kTapTimeoutMs = 2000; // a slower click starts a new series
constexpr int kMaxTaps = 8;
constexpr double kAccentWeight = 1.0;
constexpr double kPlainWeight = 0.62; // the beats inside the bar sit lower
constexpr qint64 kFrameBytes = Metronome::kChannels * qint64(sizeof(float));
} // namespace

Metronome::Metronome(QObject *parent) : QObject(parent) {
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(kChannels);
    m_format.setSampleFormat(QAudioFormat::Float);

    rebuildClicks();
    m_feeder.setInterval(kFeederMs);
    connect(&m_feeder, &QTimer::timeout, this, &Metronome::pump);
}

Metronome::~Metronome() {
    m_feeder.stop();
    if (m_sink)
        m_sink->stop();
}

// ---------------------------------------------------------------- clicks ---

QVector<float> Metronome::clickSamples(int sound, bool accent) const {
    QVector<float> out;
    const double bright = accent ? 1.25 : 1.0; // the accent is a touch higher
    auto tone = [&](double freq, double decay, double ms, double amp) {
        const int n = int(kSampleRate * ms / 1000.0);
        if (out.size() < n)
            out.resize(n);
        for (int i = 0; i < n; ++i) {
            const double t = i / double(kSampleRate);
            out[i] += float(amp * std::sin(2.0 * M_PI * freq * bright * t)
                            * std::exp(-t / decay));
        }
    };
    auto burst = [&](double decay, double ms, double amp) {
        const int n = int(kSampleRate * ms / 1000.0);
        if (out.size() < n)
            out.resize(n);
        double lowPass = 0.0;
        for (int i = 0; i < n; ++i) {
            const double t = i / double(kSampleRate);
            const double white = QRandomGenerator::global()->generateDouble() * 2.0 - 1.0;
            lowPass += 0.25 * (white - lowPass); // crude low-pass residual
            out[i] += float(amp * (white - lowPass) * std::exp(-t / decay));
        }
    };

    switch (sound) {
    case 1: // a dry hi-hat style click
        burst(0.002, 14, 0.9);
        tone(2400, 0.004, 8, 0.4);
        break;
    case 2: // a pure beep
        tone(1000, 0.03, 120, 0.9);
        break;
    case 3: // a low practice pad
        tone(460, 0.03, 110, 0.9);
        tone(920, 0.015, 60, 0.35);
        break;
    default: // a wood block knock
        tone(810, 0.008, 40, 0.9);
        tone(1620, 0.004, 24, 0.5);
        break;
    }

    // Normalize to a 1.0 peak; the pump applies gain and the accent weight.
    float peak = 0.0f;
    for (const float v : out)
        peak = qMax(peak, qAbs(v));
    if (peak > 0.0f)
        for (float &v : out)
            v /= peak;
    return out;
}

void Metronome::rebuildClicks() {
    m_click = clickSamples(m_sound, false);
    m_accent = clickSamples(m_sound, true);
}

// --------------------------------------------------------------- setters ---

void Metronome::setBpm(int bpm) {
    const int clamped = qBound(kMinBpm, bpm, kMaxBpm);
    if (clamped == m_bpm)
        return;
    m_bpm = clamped;
    emit bpmChanged();
}

void Metronome::setBeatsPerBar(int beats) {
    const int clamped = qBound(1, beats, kMaxBeats);
    if (clamped == m_beatsPerBar)
        return;
    m_beatsPerBar = clamped;
    emit beatsPerBarChanged();
}

void Metronome::setSound(int sound) {
    const int clamped = qBound(0, sound, kSoundCount - 1);
    if (clamped == m_sound)
        return;
    m_sound = clamped;
    rebuildClicks();
    emit soundChanged();
}

void Metronome::setGain(double gain) {
    const double clamped = qBound(0.0, gain, 1.0);
    if (qFuzzyCompare(clamped, m_gain))
        return;
    m_gain = clamped;
    emit gainChanged();
}

// ------------------------------------------------------------ tap tempo ---

qint64 Metronome::tapAt(qint64 ms) {
    if (!m_taps.isEmpty() && ms - m_taps.last() > kTapTimeoutMs)
        m_taps.clear(); // the series went stale; this tap starts a new one
    m_taps.append(ms);
    if (m_taps.size() > kMaxTaps)
        m_taps.removeFirst();
    if (m_taps.size() < 2)
        return m_bpm;
    qint64 sum = 0;
    for (int i = 1; i < m_taps.size(); ++i)
        sum += m_taps[i] - m_taps[i - 1];
    const double average = double(sum) / (m_taps.size() - 1);
    if (average <= 0.0)
        return m_bpm;
    setBpm(int(qRound(60000.0 / average)));
    return m_bpm;
}

void Metronome::tap() {
    if (!m_tapClock.isValid())
        m_tapClock.start();
    tapAt(m_tapClock.elapsed());
}

// ------------------------------------------------------------- lifecycle ---

bool Metronome::startSink() {
    if (!m_sink)
        m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), m_format, this);
    m_sink->stop(); // drop anything buffered from a previous pass
    m_sinkDevice = m_sink->start(); // push mode: we write into this device
    if (!m_sinkDevice ||
        (m_sink->state() == QAudio::State::StoppedState && m_sink->error() != QAudio::NoError)) {
        m_sinkDevice = nullptr;
        return false;
    }
    return true;
}

void Metronome::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    if (enabled) {
        if (QMediaDevices::defaultAudioOutput().isNull()) {
            emit errorOccurred(tr("No audio output device found."));
            return;
        }
        m_framesWritten = 0;
        m_nextClickFrame = 0; // beat one sounds immediately
        m_beatIndex = 0;
        m_carry.clear();
        if (!startSink()) {
            emit errorOccurred(tr("Audio output could not be started."));
            return;
        }
        m_enabled = true;
        emit enabledChanged();
        pump();
        m_feeder.start();
    } else {
        m_enabled = false;
        m_feeder.stop();
        m_taps.clear();
        if (m_sink)
            m_sink->stop();
        m_sinkDevice = nullptr;
        emit enabledChanged();
    }
}

void Metronome::restart() {
    if (!m_enabled || !m_sink)
        return;
    // Restarting the sink discards buffered silence, so beat one lands right
    // where playback starts instead of after the stale pipeline delay.
    m_sink->stop();
    m_sinkDevice = m_sink->start();
    m_framesWritten = 0;
    m_nextClickFrame = 0;
    m_beatIndex = 0;
    m_carry.clear();
}

// ---------------------------------------------------------------- pump ----

void Metronome::pump() {
    if (!m_enabled || !m_sink || !m_sinkDevice)
        return;
    if (m_sink->state() == QAudio::State::StoppedState && m_sink->error() != QAudio::NoError) {
        // The device failed mid-stream (unplugged, claimed by the system).
        setEnabled(false);
        emit errorOccurred(tr("Audio output error."));
        return;
    }

    while (m_enabled) {
        const qint64 freeFrames = m_sink->bytesFree() / kFrameBytes;
        if (freeFrames <= 0)
            return;
        const qint64 chunk = qMin<qint64>(freeFrames, 4096);
        m_scratch.assign(size_t(chunk) * kChannels, 0.0f);

        // A click that started in an earlier chunk finishes first.
        const qint64 carried = qMin<qint64>(m_carry.size(), chunk);
        for (qint64 i = 0; i < carried; ++i) {
            m_scratch[size_t(i) * kChannels] += m_carry[int(i)];
            m_scratch[size_t(i) * kChannels + 1] += m_carry[int(i)];
        }
        if (carried < qint64(m_carry.size()))
            m_carry.remove(0, int(carried));
        else
            m_carry.clear();

        // Schedule every beat whose start frame falls inside this chunk; the
        // absolute frame counters keep the placement exact across ticks.
        while (m_nextClickFrame < m_framesWritten + chunk) {
            if (m_nextClickFrame >= m_framesWritten) {
                const qint64 offset = m_nextClickFrame - m_framesWritten;
                const bool accent = m_beatsPerBar >= 2 && m_beatIndex % m_beatsPerBar == 0;
                const QVector<float> &click = accent ? m_accent : m_click;
                const float scale = float(m_gain
                                          * (accent ? kAccentWeight : kPlainWeight));
                const qint64 take = qMin<qint64>(click.size(), chunk - offset);
                for (qint64 i = 0; i < take; ++i) {
                    const float s = click[int(i)] * scale;
                    m_scratch[size_t(offset + i) * kChannels] += s;
                    m_scratch[size_t(offset + i) * kChannels + 1] += s;
                }
                if (take < qint64(click.size())) {
                    // The click crosses the chunk border; remember the rest.
                    m_carry.resize(int(click.size() - take));
                    for (int i = 0; i < m_carry.size(); ++i)
                        m_carry[i] = click[int(take + i)] * scale;
                }
            }
            ++m_beatIndex;
            m_nextClickFrame += qRound64(kSampleRate * 60.0 / m_bpm);
        }

        const qint64 n = m_sinkDevice->write(
            reinterpret_cast<const char *>(m_scratch.data()), chunk * kFrameBytes);
        const qint64 frames = qMax<qint64>(0, n) / kFrameBytes;
        if (frames <= 0)
            return; // no room yet; the same chunk is rebuilt on the next tick
        m_framesWritten += frames;
        if (frames < chunk)
            return; // the sink took part; the remainder is rebuilt next tick
    }
}
