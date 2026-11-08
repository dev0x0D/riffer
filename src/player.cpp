#include "player.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#include <SoundTouch.h>

// A pull-mode source for QAudioSink: fills from SoundTouch, which is fed
// from the in-memory buffer and wraps at the loop end without a seam.
class Player::Stream : public QIODevice {
public:
    explicit Stream(Player *player) : m_player(player) { open(ReadWrite); }

    qint64 readData(char *data, qint64 maxlen) override {
        if (maxlen <= 0 || !m_player->m_frames)
            return 0;
        const qint64 frames = maxlen / (Player::kChannels * qint64(sizeof(float)));
        qint64 got = 0;
        m_player->fill(frames);
        while (got < frames) {
            const uint available = m_player->m_st->numSamples();
            if (!available)
                break;
            const uint take = uint(qMin<qint64>(frames - got, available));
            const uint n = m_player->m_st->receiveSamples(
                reinterpret_cast<float *>(data) + size_t(got) * Player::kChannels, take);
            if (!n)
                break;
            got += n;
        }
        m_player->m_outFrames += got;
        return got * Player::kChannels * qint64(sizeof(float));
    }

    qint64 writeData(const char *, qint64) override { return 0; }
    qint64 bytesAvailable() const override {
        return qint64(m_player->m_st->numSamples()) * Player::kChannels * qint64(sizeof(float));
    }

private:
    Player *m_player;
};

Player::Player(QObject *parent) : QObject(parent) {
    m_format.setSampleRate(kSampleRate);
    m_format.setChannelCount(kChannels);
    m_format.setSampleFormat(QAudioFormat::Float);

    m_st = new soundtouch::SoundTouch;
    m_st->setSampleRate(kSampleRate);
    m_st->setChannels(kChannels);
    m_st->setTempo(1.0);

    m_stream = new Stream(this);
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
    if (m_playing) {
        // Record the change where it becomes audible, not where it is fed:
        // SoundTouch still holds samples stretched at the old tempo.
        m_st->setTempo(s);
        m_clock.setTempo(m_outFrames + latencyOutFrames(), m_speed);
    } else {
        m_st->setTempo(s);
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

void Player::play() {
    if (!m_frames || m_frameCount <= 0 || m_loopEndF <= m_loopStartF)
        return;
    if (QMediaDevices::defaultAudioOutput().isNull()) {
        emit errorOccurred(tr("No audio output device found."));
        return;
    }
    if (!m_sink) {
        m_sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), m_format, this);
        m_sink->setVolume(float(m_volume));
    }
    m_sink->stop(); // Drop anything buffered from a previous pass.
    resetPipeline(m_loopStartF);
    m_sink->start(m_stream);
    if (m_sink->error() != QAudio::NoError) {
        emit errorOccurred(tr("Audio output could not be started."));
        return;
    }
    m_playing = true;
    emit playingChanged();
}

void Player::stop() {
    if (!m_playing)
        return;
    if (m_sink)
        m_sink->stop();
    m_st->clear();
    m_playing = false;
    emit playingChanged();
}

void Player::seek(double sec) {
    if (!m_playing || !m_frames)
        return;
    // Re-starting the pipeline at the new frame is simpler and cleaner than
    // patching state mid-stream: the sink drops its ~100 ms buffer too.
    const qint64 frame = frameOf(sec);
    m_sink->stop();
    resetPipeline(frame);
    m_sink->start(m_stream);
}

double Player::inputPosition() const {
    if (!m_playing)
        return loopStart();
    qint64 out = m_outFrames;
    // Frames sitting in the device buffer have not been heard yet.
    if (m_sink && m_sink->bufferSize() > 0) {
        const qint64 buffered =
            (m_sink->bufferSize() - m_sink->bytesFree()) / (kChannels * qint64(sizeof(float)));
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
    // It is a #define in SoundTouch.h, so it carries no namespace.
    const qint64 initial = m_st->getSetting(SETTING_INITIAL_LATENCY);
    const qint64 unprocessed = qint64(m_st->numUnprocessedSamples());
    return qint64((initial + unprocessed) / qMax(0.05, m_speed));
}

void Player::fill(qint64 framesWanted) {
    while (qint64(m_st->numSamples()) < framesWanted) {
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
