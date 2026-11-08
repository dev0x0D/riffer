#include "overviewbar.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr qreal kInset = 4; // margin between the item and the drawn frame
constexpr qreal kRadius = 5;
} // namespace

OverviewBar::OverviewBar(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setCursor(QCursor(Qt::PointingHandCursor));
}

void OverviewBar::setTrack(Backend *backend) {
    if (m_backend == backend)
        return;
    m_backend = backend;
    emit trackChanged();
    watch();
    update();
}

void OverviewBar::watch() {
    for (const QMetaObject::Connection &connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    if (!m_backend)
        return;
    m_connections.append(connect(m_backend, &Backend::trackChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::playheadChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::selectionChanged, this, [this] { update(); }));
    m_connections.append(connect(m_backend, &Backend::sectionsChanged, this, [this] { update(); }));
}

void OverviewBar::setViewStart(double start) {
    if (qFuzzyCompare(start, m_viewStart))
        return;
    m_viewStart = start;
    emit viewChanged();
    update();
}

void OverviewBar::setViewEnd(double end) {
    if (qFuzzyCompare(end, m_viewEnd))
        return;
    m_viewEnd = end;
    emit viewChanged();
    update();
}

void OverviewBar::setAccent(const QColor &color) {
    m_accent = color;
    emit themeChanged();
    update();
}

double OverviewBar::timeAt(qreal x) const {
    const double duration = m_backend ? m_backend->duration() : 0;
    if (duration <= 0)
        return 0;
    const qreal innerW = qMax<qreal>(1, width() - 2 * kInset);
    return qBound(0.0, double(x - kInset) / innerW * duration, duration);
}

void OverviewBar::mousePressEvent(QMouseEvent *event) {
    m_panning = true;
    const double t = timeAt(event->position().x());
    const double span = m_viewEnd - m_viewStart;
    if (t >= m_viewStart && t <= m_viewEnd) {
        // Grab the window and drag it 1:1, keeping the zoom.
        m_grabOffset = t - m_viewStart;
    } else {
        // Clicking outside jumps the window so the pointer is its center,
        // then keeps dragging it by the middle. The zoom stays untouched.
        m_grabOffset = span / 2;
        emit viewPanRequested(t - m_grabOffset);
    }
}

void OverviewBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_panning)
        emit viewPanRequested(timeAt(event->position().x()) - m_grabOffset);
}

void OverviewBar::mouseReleaseEvent(QMouseEvent *) {
    m_panning = false;
}

void OverviewBar::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal w = width(), h = height();
    const QRectF frame(kInset, kInset, w - 2 * kInset, h - 2 * kInset);
    painter->setPen(QPen(QColor(255, 255, 255, 20), 1));
    painter->setBrush(QColor(255, 255, 255, 7));
    painter->drawRoundedRect(frame, kRadius, kRadius);
    if (!m_backend || !m_backend->ready() || frame.width() < 4)
        return;

    const double duration = m_backend->duration();
    const qreal innerX = frame.x();
    const qreal innerW = frame.width();
    const auto xOf = [&](double t) { return innerX + t / duration * innerW; };
    const qreal midY = frame.y() + frame.height() / 2;
    const qreal amp = frame.height() / 2 - 4;

    // ------------------------------------------------------------ wave ---
    const qint64 frameCount = qint64(m_backend->frames().size()) / Player::kChannels;
    if (frameCount > 0) {
        const std::vector<PeakLevel> &peaks = m_backend->peaks();
        const double framesPerColumn = duration / innerW * Player::kSampleRate;
        const int level = levelForBucket(peaks, qint64(framesPerColumn));
        const float *frames = m_backend->frames().data();
        const QColor played(255, 255, 255, 70);
        QColor wave = m_accent;
        wave.setAlpha(200);
        const qreal playX = xOf(m_backend->playhead());
        for (int col = 0; col < int(innerW); ++col) {
            const double f0 = double(col) / innerW * frameCount;
            const double f1 = (double(col) + 1) / innerW * frameCount;
            float lo = 0, hi = 0;
            if (level >= 0 && framesPerColumn >= 256) {
                const PeakLevel &lv = peaks[size_t(level)];
                const qint64 b0 = qint64(f0 / lv.bucketFrames);
                const qint64 b1 = qint64(f1 / lv.bucketFrames);
                const Peak peak = combinePeaks(lv, b0, qMax<qint64>(1, b1 - b0));
                lo = peak.minV;
                hi = peak.maxV;
            } else {
                const qint64 begin = qBound<qint64>(0, qint64(f0), frameCount - 1);
                const qint64 end = qBound<qint64>(begin + 1, qint64(f1), frameCount);
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
            const qreal top = midY - qBound(0.0f, hi, 1.0f) * amp;
            const qreal bottom = midY - qBound(-1.0f, lo, 0.0f) * amp;
            painter->fillRect(QRectF(innerX + col, top, 1.0, qMax(1.0, bottom - top)),
                              innerX + qreal(col) < playX ? played : wave);
        }
    }

    // -------------------------------------------------- selection dim-out ---
    if (m_backend->hasSelection()) {
        const qreal x1 = xOf(m_backend->selectionStart());
        const qreal x2 = xOf(m_backend->selectionEnd());
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 110));
        if (x1 > innerX)
            painter->drawRect(QRectF(innerX, frame.y(), x1 - innerX, frame.height()));
        if (x2 < innerX + innerW)
            painter->drawRect(QRectF(x2, frame.y(), innerX + innerW - x2, frame.height()));
    }

    // -------------------------------------------------------- sections ---
    painter->setPen(QPen(m_accent, 2));
    for (const Backend::Section &section : m_backend->sectionList()) {
        const qreal x = xOf(section.start);
        painter->drawLine(QPointF(x, frame.y() + 2), QPointF(x, frame.y() + frame.height() - 2));
    }

    // --------------------------------------------------- viewport rect ---
    const qreal vx1 = xOf(qBound(0.0, m_viewStart, duration));
    const qreal vx2 = xOf(qBound(0.0, m_viewEnd, duration));
    painter->setPen(QPen(m_accent, 1));
    painter->setBrush(QColor(255, 255, 255, 26));
    painter->drawRoundedRect(QRectF(vx1, frame.y(), qMax<qreal>(8, vx2 - vx1), frame.height()), 3, 3);

    // -------------------------------------------------------- playhead ---
    painter->setPen(QPen(QColor(255, 255, 255, 220), 1));
    const qreal playX = xOf(m_backend->playhead());
    painter->drawLine(QPointF(playX, frame.y() + 1), QPointF(playX, frame.y() + frame.height() - 1));
}
