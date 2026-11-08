#include "player.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <SoundTouch.h>

namespace {
// Feed the sink roughly every 20 ms; its buffer (~100 ms) absorbs jitter.
constexpr int kFeederMs = 20;
constexpr qint64 kFrameBytes = Player::kChannels * qint64(sizeof(float));
} // namespace

Player::Player(QObject *parent) : QObject(parent) {
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(kChannels);
    m_format.setSampleFormat(QAudioFormat::Float);

    m_st = new soundtouch::SoundTouch;
    m_st->setSampleRate(kSampleRate);
    m_st->setChannels(kChannels);
    m_st->setTempo(1.0);

    m_feeder.setInterval(kFeederMs);
    connect(&m_feeder, &QTimer::timeout, this, &Player::pump);
}

Player::~Player() {
    stop();
    delete m_st;
}

void Player::setSource(const float *frames, qint64 frameCount) {
    stop();
    m_frames = frames;
    m_frameCount = frameCount;
    m_loopStartF = 0;
    m_loopEndF = frameCount;
}

void Player::setLoop(double startSec, double endSec) {
    const qint64 a = frameOf(startSec);
    const qint64 b = qBound(a, frameOf(endSec), m_frameCount);
    m_loopStartF = a;
    m_loopEndF = qMax(a + 1, b);
    // A loop that shrinks past the playhead has to pull the pipeline back
    // into it, or the next wrap would run to the end of the track first.
    if (m_playing && (m_nextInput < m_loopStartF || m_nextInput >= m_loopEndF))
        resetPipeline(m_loopStartF);
}

void Player::setSpeed(double speed) {
    const double s = qBound(0.2, speed, 1.5);
    if (qFuzzyCompare(s, m_speed))
        return;
    m_speed = s;
    m_st->setTempo(s);
    if (m_playing) {
        // Record the change where it becomes audible, not where it is fed:
        // SoundTouch still holds samples stretched at the old tempo.
        m_clock.setTempo(m_outFrames + latencyOutFrames(), m_speed);
    }
    emit speedChanged(s);
}

void Player::setVolume(double volume) {
    const double v = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(v, m_volume))
        return;
    m_volume = v;
    if (m_sink)
        m_sink->setVolume(float(v));
    emit volumeChanged(v);
}

bool Player::startSink() {
    if (!m_sink) {
        m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), m_format, this);
        m_sink->setVolume(float(m_volume));
    }
    m_sink->stop(); // Drop anything buffered from a previous pass.
    m_sinkDevice = m_sink->start(); // push mode: we write into this device
    if (!m_sinkDevice ||
        (m_sink->state() == QAudio::State::StoppedState && m_sink->error() != QAudio::NoError)) {
        m_sinkDevice = nullptr;
        return false;
    }
    return true;
}

void Player::play() {
    if (!m_frames || m_frameCount <= 0 || m_loopEndF <= m_loopStartF)
        return;
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        emit errorOccurred(tr("No audio output device found."));
        return;
    }
    m_pending.clear();
    resetPipeline(m_loopStartF);
    if (!startSink()) {
        emit errorOccurred(tr("Audio output could not be started."));
        return;
    }
    m_playing = true;
    emit playingChanged();
    pump();
    m_feeder.start();
}

void Player::stop() {
    m_feeder.stop();
    m_pending.clear();
    if (!m_playing)
        return;
    if (m_sink)
        m_sink->stop();
    m_sinkDevice = nullptr;
    m_st->clear();
    m_playing = false;
    emit playingChanged();
}

void Player::seek(double sec) {
    if (!m_playing || !m_frames)
        return;
    // Re-starting the pipeline at the new frame is simpler and cleaner than
    // patching state mid-stream: the sink drops its buffer too.
    const qint64 frame = frameOf(sec);
    m_pending.clear();
    m_sink->stop();
    resetPipeline(frame);
    if (!startSink()) {
        m_feeder.stop();
        m_playing = false;
        emit playingChanged();
        emit errorOccurred(tr("Audio output could not be started."));
        return;
    }
    pump();
}

double Player::inputPosition() const {
    if (!m_playing)
        return loopStart();
    qint64 out = m_outFrames;
    // Frames sitting in the sink buffer have not been heard yet.
    if (m_sink && m_sink->bufferSize() > 0) {
        const qint64 buffered = (m_sink->bufferSize() - m_sink->bytesFree()) / kFrameBytes;
        out -= qMin(out, buffered);
    }
    return qMax(0.0, m_clock.inputAt(qMax<qint64>(0, out))) / kSampleRate;
}

void Player::resetPipeline(qint64 startFrame) {
    m_st->clear();
    m_st->setTempo(m_speed);
    m_nextInput = startFrame;
    m_outFrames = 0;
    m_clock.reset(double(startFrame), m_speed);
}

qint64 Player::latencyOutFrames() const {
    // SETTING_INITIAL_LATENCY is a #define in SoundTouch.h, no namespace.
    const qint64 initial = m_st->getSetting(SETTING_INITIAL_LATENCY);
    const qint64 unprocessed = qint64(m_st->numUnprocessedSamples());
    return qint64((initial + unprocessed) / qMax(0.05, m_speed));
}

// Feeds SoundTouch enough input to cover the sink's free space plus the
// stretch pipeline's own latency. The bound counts processed and queued
// input together, so the loop always terminates: every pushed chunk moves
// the sum forward while the target stays fixed.
void Player::fill(qint64 framesWanted) {
    const qint64 pipelineLatency =
        qint64(m_st->getSetting(SETTING_INITIAL_LATENCY) / qMax(0.05, m_speed));
    const qint64 target = framesWanted + pipelineLatency;
    while (qint64(m_st->numSamples()) + qint64(m_st->numUnprocessedSamples()) < target) {
        qint64 remain = m_loopEndF - m_nextInput;
        if (remain <= 0) {
            m_nextInput = m_loopStartF;
            // The wrap is heard about one pipeline latency after it is fed,
            // so timestamp the clock at that future output frame.
            m_clock.wrapped(m_outFrames + latencyOutFrames(), double(m_loopStartF));
            remain = m_loopEndF - m_loopStartF;
            if (remain <= 0)
                break;
        }
        const uint chunk = uint(qMin<qint64>(remain, 8192));
        m_st->putSamples(m_frames + size_t(m_nextInput) * kChannels, chunk);
        m_nextInput += chunk;
    }
}

void Player::pump() {
    if (!m_playing || !m_sink || !m_sinkDevice)
        return;
    if (m_sink->state() == QAudio::State::StoppedState && m_sink->error() != QAudio::NoError) {
        // The device failed mid-stream (unplugged, claimed by the system).
        stop();
        emit errorOccurred(tr("Audio output error."));
        return;
    }

    // First deliver anything the sink refused on an earlier tick.
    if (!m_pending.empty()) {
        const qint64 n = m_sinkDevice->write(
            reinterpret_cast<const char *>(m_pending.data()), qint64(m_pending.size()) * kFrameBytes);
        if (n <= 0)
            return; // no room yet; retry on the next tick
        const qint64 frames = n / kFrameBytes;
        m_outFrames += frames;
        m_pending.erase(m_pending.begin(), m_pending.begin() + qint64(frames) * kChannels);
    }

    const qint64 freeFrames = m_sink->bytesFree() / kFrameBytes;
    if (freeFrames <= 0)
        return;
    fill(freeFrames);
    const uint avail = m_st->numSamples();
    if (avail <= 0)
        return; // the stretch pipeline is still warming up
    const uint take = uint(qMin<qint64>(avail, freeFrames));
    if (m_scratch.size() < size_t(take) * kChannels)
        m_scratch.resize(size_t(take) * kChannels);
    const uint got = m_st->receiveSamples(m_scratch.data(), take);
    if (got == 0)
        return;
    const qint64 n = m_sinkDevice->write(
        reinterpret_cast<const char *>(m_scratch.data()), qint64(got) * kFrameBytes);
    const qint64 frames = qMax<qint64>(0, n) / kFrameBytes;
    m_outFrames += frames;
    if (frames < qint64(got)) {
        // The sink did not take everything; carry the rest to the next tick.
        m_pending.insert(m_pending.end(), m_scratch.begin() + qint64(frames) * kChannels,
                         m_scratch.begin() + qint64(got) * kChannels);
    }
}
