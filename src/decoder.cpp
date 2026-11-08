#include "decoder.h"

#include <QFileInfo>
#include <QStandardPaths>

Decoder::Decoder(const QString &ffmpeg, const QString &ffprobe, QObject *parent)
    : QObject(parent),
      m_ffmpeg(ffmpeg.isEmpty() ? QStandardPaths::findExecutable("ffmpeg") : ffmpeg),
      m_ffprobe(ffprobe.isEmpty() ? QStandardPaths::findExecutable("ffprobe") : ffprobe) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_partial.append(m_process.readAllStandardOutput());
        const size_t whole = size_t(m_partial.size()) / 4;
        if (whole) {
            const float *samples = reinterpret_cast<const float *>(m_partial.constData());
            m_frames.insert(m_frames.end(), samples, samples + whole);
            m_partial.remove(0, int(whole * 4));
        }
        if (m_duration > 0) {
            const double fraction = qBound(
                0.0, double(m_frames.size() / kChannels) / (m_duration * kSampleRate), 1.0);
            const int percent = int(fraction * 100);
            if (percent != m_lastPercent) {
                m_lastPercent = percent;
                emit progressed(fraction);
            }
        }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (m_state != State::Decoding)
            return; // The request was cancelled; its result is discarded.
        m_state = State::Idle;
        if (code == 0 && status == QProcess::NormalExit && m_frames.size() >= size_t(kChannels)) {
            emit decoded(m_path, double(m_frames.size() / kChannels) / kSampleRate);
        } else {
            const QString detail = QString::fromUtf8(m_process.readAllStandardError())
                                       .section('\n', -2)
                                       .trimmed();
            fail(detail.isEmpty() ? tr("ffmpeg exited with code %1.").arg(code)
                                  : tr("ffmpeg failed: %1").arg(detail));
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_state == State::Decoding)
            fail(tr("Could not run ffmpeg. Is it installed?"));
    });
}

Decoder::~Decoder() {
    cancel();
}

double Decoder::probeDurationBlocking(const QString &path) const {
    QProcess probe;
    probe.start(m_ffprobe,
                {"-v", "error", "-show_entries", "format=duration", "-of",
                 "default=noprint_wrappers=1:nokey=1", path});
    if (!probe.waitForStarted(2000) || !probe.waitForFinished(3000)) {
        probe.kill();
        probe.waitForFinished(200);
        return -1;
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0)
        return -1;
    bool ok = false;
    const double seconds = QString::fromUtf8(probe.readAllStandardOutput()).trimmed().toDouble(&ok);
    return ok && seconds > 0 ? seconds : -1;
}

void Decoder::load(const QString &path) {
    if (m_ffmpeg.isEmpty()) {
        emit failed(tr("ffmpeg is not installed."));
        return;
    }
    cancel();
    m_path = path;
    m_duration = probeDurationBlocking(path);
    m_frames.clear();
    m_partial.clear();
    m_lastPercent = -1;
    if (m_duration > 0) {
        // Reserve up front so a long track does not re-copy as it grows.
        try {
            m_frames.reserve(size_t(m_duration * kSampleRate * kChannels));
        } catch (const std::bad_alloc &) {
            // Growing dynamically still works; this was only an optimization.
        }
    }
    m_state = State::Decoding;
    // -vn keeps album art in MP3s out of the pipeline; everything is decoded
    // to one fixed format so playback and the waveform share one buffer.
    m_process.start(m_ffmpeg,
                    {"-hide_banner", "-loglevel", "error", "-nostdin", "-i", path, "-vn", "-map",
                     "0:a:0", "-f", "f32le", "-acodec", "pcm_f32le", "-ar",
                     QByteArray::number(kSampleRate), "-ac", QByteArray::number(kChannels),
                     "pipe:1"});
}

void Decoder::cancel() {
    if (m_state == State::Decoding) {
        m_state = State::Idle;
        m_process.kill();
        m_process.waitForFinished(200);
    }
}

void Decoder::fail(const QString &message) {
    m_state = State::Idle;
    emit failed(tr("Could not decode %1: %2").arg(QFileInfo(m_path).fileName(), message));
}
