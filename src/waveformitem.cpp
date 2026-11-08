#include "waveformitem.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kRulerHeight = 26;
constexpr int kLaneHeight = 22;

// Ruler label steps, seconds; the first wide enough on screen wins.
const double kSteps[] = {0.05, 0.1, 0.25, 0.5, 1, 2, 5, 10, 15, 30, 60,
                         120, 300, 600, 1200, 1800, 3600};
} // namespace

WaveformView::WaveformView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}

void WaveformView::setBackend(Backend *backend) {
    if (m_backend == backend)
        return;
    m_backend = backend;
    emit backendChanged();
    watchBackend();
    update();
}

void WaveformView::watchBackend() {
    for (const QMetaObject::Connection &connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    if (!m_backend)
        return;
    // Any state the paint reads can change what a frame looks like.
    m_connections.append(connect(m_backend, &Backend::trackChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::playheadChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::cursorChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::selectionChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::loopChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::sectionsChanged, this, [this] { update(); }));
    m_connections.append(
        connect(m_backend, &Backend::sectionPendingChanged, this, [this] { update(); }));
}

void WaveformView::setViewStart(double start) {
    if (qFuzzyCompare(start, m_viewStart))
        return;
    m_viewStart = start;
    emit viewChanged();
    update();
}

void WaveformView::setViewEnd(double end) {
    if (qFuzzyCompare(end, m_viewEnd))
        return;
    m_viewEnd = end;
    emit viewChanged();
    update();
}

void WaveformView::setAccent(const QColor &color) {
    m_accent = color;
    emit themeChanged();
    update();
}

void WaveformView::setAccentForeground(const QColor &color) {
    m_accentForeground = color;
    emit themeChanged();
    update();
}

void WaveformView::setRadius(int radius) {
    m_radius = radius;
    emit themeChanged();
    update();
}

void WaveformView::paint(QPainter *painter) {
    if (!m_backend || !m_backend->ready())
        return;

    const double duration = m_backend->duration();
    const double vs = qBound(0.0, m_viewStart, duration);
    const double ve = qBound(vs + 0.001, m_viewEnd, duration);
    const qreal w = width();
    if (w < 2)
        return;
    const double pxPerSec = w / (ve - vs);
    const auto xOf = [&](double t) { return (t - vs) * pxPerSec; };

    painter->setRenderHint(QPainter::Antialiasing);
    QFont font = painter->font();
    font.setPixelSize(10);
    painter->setFont(font);

    const QColor subtleLine(255, 255, 255, 26);
    const QColor labelColor(255, 255, 255, 150);

    // ------------------------------------------------------------- ruler ---
    {
        painter->fillRect(QRectF(0, 0, w, kRulerHeight), QColor(255, 255, 255, 10));
        double step = kSteps[sizeof(kSteps) / sizeof(kSteps[0]) - 1];
        for (double candidate : kSteps) {
            if (candidate * pxPerSec >= 78) {
                step = candidate;
                break;
            }
        }
        const double minor = step / 5;
        QPen pen(subtleLine, 1);
        painter->setPen(pen);
        if (minor * pxPerSec >= 11) {
            for (double t = std::ceil(vs / minor) * minor; t <= ve; t += minor) {
                const qreal x = qRound(xOf(t)) + 0.5;
                painter->drawLine(QPointF(x, kRulerHeight - 5), QPointF(x, kRulerHeight));
            }
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 70));
        for (double t = std::ceil(vs / step) * step; t <= ve; t += step) {
            const qreal x = qRound(xOf(t)) + 0.5;
            painter->drawRect(QRectF(x, kRulerHeight - 9, 1, 9));
            painter->setPen(labelColor);
            const bool fraction = step < 1;
            painter->drawText(QRectF(x + 4, 0, 120, kRulerHeight - 8), Qt::AlignVCenter | Qt::AlignLeft,
                              m_backend->formatTime(t, fraction));
            painter->setPen(Qt::NoPen);
        }
        painter->setPen(QPen(subtleLine, 1));
        painter->drawLine(QPointF(0, kRulerHeight + 0.5), QPointF(w, kRulerHeight + 0.5));
    }

    const double playhead = m_backend->playhead();
    const qreal playX = xOf(playhead);

    // ------------------------------------------------------ sections lane ---
    {
        const QVector<Backend::Section> &sections = m_backend->sectionList();
        for (const Backend::Section &section : sections) {
            if (section.end < vs || section.start > ve)
                continue;
            const qreal x1 = xOf(section.start);
            const qreal x2 = xOf(section.end);
            const bool active = playhead >= section.start && playhead < section.end;
            QRectF chip(qMax<qreal>(x1, 0), kRulerHeight + 3,
                        qMax<qreal>(qMin(x2, w) - qMax(x1, 0.0), 6), kLaneHeight - 8);
            painter->setPen(Qt::NoPen);
            painter->setBrush(active ? m_accent : QColor(m_accent.red(), m_accent.green(), m_accent.blue(), 70));
            painter->drawRoundedRect(chip, qMin<qreal>(chip.height() / 2, 5), qMin<qreal>(chip.height() / 2, 5));
            if (chip.width() >= 36) {
                painter->setPen(active ? m_accentForeground : QColor(255, 255, 255, 210));
                QFont small = painter->font();
                small.setPixelSize(11);
                painter->setFont(small);
                QFontMetrics metrics(small);
                const QString name = metrics.elidedText(section.name, Qt::ElideRight,
                                                        int(chip.width()) - 12);
                painter->drawText(chip.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, name);
            }
        }
    }

    // ---------------------------------------------------------- waveform ---
    const qint64 frameCount = qint64(m_backend->frames().size()) / Player::kChannels;
    if (frameCount > 0) {
        const double rate = Player::kSampleRate;
        const double perColumn = qMax(1.0, (ve - vs) * rate / w);
        const std::vector<PeakLevel> &peaks = m_backend->peaks();
        const int level = levelForBucket(peaks, qint64(perColumn));

        const qreal waveTop = kRulerHeight + kLaneHeight;
        const qreal waveHeight = height() - waveTop - 8;
        const qreal center = waveTop + waveHeight / 2;
        const qreal amp = waveHeight / 2 - 2;
        const float *frames = m_backend->frames().data();

        const QColor played(255, 255, 255, 52);
        for (int col = 0; col < int(w); ++col) {
            const double f0 = (vs + col / w * (ve - vs)) * rate;
            const double f1 = f0 + perColumn;
            float lo = 0, hi = 0;
            if (level >= 0 && perColumn >= 256) {
                const PeakLevel &LV = peaks[size_t(level)];
                const qint64 b0 = qint64(f0 / LV.bucketFrames);
                const qint64 b1 = qint64(f1 / LV.bucketFrames);
                const Peak peak = combinePeaks(LV, b0, qMax<qint64>(1, b1 - b0));
                lo = peak.minV;
                hi = peak.maxV;
            } else {
                qint64 begin = qBound<qint64>(0, qint64(f0), frameCount - 1);
                qint64 end = qBound<qint64>(begin + 1, qint64(f1), frameCount);
                lo = 1;
                hi = -1;
                for (qint64 f = begin; f < end; ++f) {
                    const float v = (frames[f * 2] + frames[f * 2 + 1]) / 2.0f;
                    lo = qMin(lo, v);
                    hi = qMax(hi, v);
                }
                if (lo > hi)
                    lo = hi = 0;
            }
            const qreal top = center - qBound(0.0f, hi, 1.0f) * amp;
            const qreal bottom = center - qBound(-1.0f, lo, 0.0f) * amp;
            // Played columns fade back so the wave ahead stays the focus.
            painter->fillRect(QRectF(col, top, 1.0, qMax(1.0, bottom - top)),
                              qreal(col) < playX ? played : m_accent);
        }
    }

    // --------------------------------------------------------- selection ---
    if (m_backend->hasSelection()) {
        const double ss = m_backend->selectionStart();
        const double se = m_backend->selectionEnd();
        const qreal x1 = xOf(ss);
        const qreal x2 = xOf(se);
        // Everything outside the loop dims, so the loop
        // reads at a glance.
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 90));
        if (x1 > 0)
            painter->drawRect(QRectF(0, kRulerHeight, x1, height() - kRulerHeight));
        if (x2 < w)
            painter->drawRect(QRectF(x2, kRulerHeight, w - x2, height() - kRulerHeight));
        painter->setBrush(QColor(255, 255, 255, 26));
        painter->drawRect(QRectF(qMax<qreal>(x1, 0), kRulerHeight,
                                 qMax<qreal>(qMin(x2, w) - qMax(x1, 0.0), 1),
                                 height() - kRulerHeight));
        painter->setPen(QPen(m_accent, 1));
        painter->drawLine(QPointF(x1, kRulerHeight), QPointF(x1, height()));
        painter->drawLine(QPointF(x2, kRulerHeight), QPointF(x2, height()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 220));
        painter->drawRoundedRect(QRectF(x1 - 2, kRulerHeight + height() * 0.25, 4, height() * 0.5), 2, 2);
        painter->drawRoundedRect(QRectF(x2 - 2, kRulerHeight + height() * 0.25, 4, height() * 0.5), 2, 2);
    }

    // ------------------------------------------------------------ markers ---
    auto dashedAccent = [&]() {
        QPen pen(m_accent, 1, Qt::DashLine);
        return pen;
    };

    if (m_backend->cursor() > 0.001) {
        const qreal x = qRound(xOf(m_backend->cursor())) + 0.5;
        painter->setPen(dashedAccent());
        painter->drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
    }

    if (m_backend->sectionPending()) {
        const qreal x = qRound(xOf(m_backend->sectionPendingAt())) + 0.5;
        QPen pen(QColor(255, 255, 255, 200), 1, Qt::DotLine);
        painter->setPen(pen);
        painter->drawLine(QPointF(x, kRulerHeight), QPointF(x, height()));
        painter->setPen(QColor(255, 255, 255, 220));
        QFont small = painter->font();
        small.setPixelSize(11);
        small.setBold(true);
        painter->setFont(small);
        painter->drawText(QRectF(x + 4, height() - 20, 160, 16), Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("M"));
    }

    // ----------------------------------------------------------- playhead ---
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 235));
        QPainterPath triangle;
        triangle.moveTo(playX - 5, 6);
        triangle.lineTo(playX + 5, 6);
        triangle.lineTo(playX, 15);
        triangle.closeSubpath();
        painter->drawPath(triangle);
        painter->setPen(QPen(QColor(255, 255, 255, 235), 1.5));
        painter->drawLine(QPointF(playX, 15), QPointF(playX, height()));
    }
}
