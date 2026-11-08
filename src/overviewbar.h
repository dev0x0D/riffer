#pragma once

#include <QColor>
#include <QQuickPaintedItem>
#include <QVector>

#include "backend.h"

// A thin whole-track strip under the main wave: the entire file at a glance
// with a viewport rectangle showing which part the main view currently
// displays. Clicking or dragging it pans the main view; zooming itself stays
// on the main wave (Ctrl+wheel, +/-, F, 0).
class OverviewBar : public QQuickPaintedItem {
    Q_OBJECT
    // Named `track`, like WaveformView, so the binding cannot shadow the
    // `backend` context property.
    Q_PROPERTY(Backend *track READ track WRITE setTrack NOTIFY trackChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY themeChanged)

public:
    explicit OverviewBar(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    Backend *track() const { return m_backend; }
    void setTrack(Backend *backend);
    double viewStart() const { return m_viewStart; }
    void setViewStart(double start);
    double viewEnd() const { return m_viewEnd; }
    void setViewEnd(double end);
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &color);

signals:
    void trackChanged();
    void viewChanged();
    void themeChanged();
    // The proposed new left edge of the view window; the span never changes,
    // so panning the strip cannot alter the main view's zoom.
    void viewPanRequested(double newViewStart);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void watch();
    double timeAt(qreal x) const;

    Backend *m_backend = nullptr;
    double m_viewStart = 0;
    double m_viewEnd = 1;
    QColor m_accent = QColor("#FFD60A");
    bool m_panning = false;
    double m_grabOffset = 0; // pointer position inside the window, seconds
    QVector<QMetaObject::Connection> m_connections;
};
