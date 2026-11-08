#pragma once

#include <QColor>
#include <QQuickPaintedItem>

#include "backend.h"

// Paints everything time-based for the visible window: the time ruler, the
// sections lane, the waveform columns (from the peaks pyramid, or the raw
// samples when zoomed deep), the loop selection, the cursor, the pending
// section marker and the playhead. All interaction lives in QML on top.
class WaveformView : public QQuickPaintedItem {
    Q_OBJECT
    // Named `track`, not `backend`: a property called `backend` would shadow
    // the `backend` context property in the QML binding and self-assign null.
    Q_PROPERTY(Backend *track READ backend WRITE setBackend NOTIFY backendChanged)
    Q_PROPERTY(double viewStart READ viewStart WRITE setViewStart NOTIFY viewChanged)
    Q_PROPERTY(double viewEnd READ viewEnd WRITE setViewEnd NOTIFY viewChanged)
    Q_PROPERTY(QColor accent READ accent WRITE setAccent NOTIFY themeChanged)
    Q_PROPERTY(QColor accentForeground READ accentForeground WRITE setAccentForeground NOTIFY
                   themeChanged)
    Q_PROPERTY(int radius READ radius WRITE setRadius NOTIFY themeChanged)

public:
    explicit WaveformView(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    Backend *backend() const { return m_backend; }
    void setBackend(Backend *backend);

    double viewStart() const { return m_viewStart; }
    void setViewStart(double start);
    double viewEnd() const { return m_viewEnd; }
    void setViewEnd(double end);
    QColor accent() const { return m_accent; }
    void setAccent(const QColor &color);
    QColor accentForeground() const { return m_accentForeground; }
    void setAccentForeground(const QColor &color);
    int radius() const { return m_radius; }
    void setRadius(int radius);

signals:
    void backendChanged();
    void viewChanged();
    void themeChanged();

private:
    void watchBackend();

    Backend *m_backend = nullptr;
    QVector<QMetaObject::Connection> m_connections;
    double m_viewStart = 0;
    double m_viewEnd = 1;
    QColor m_accent = QColor("#FFD60A");
    QColor m_accentForeground = Qt::black;
    int m_radius = 0;
};
