// Adapted from Omacut (MIT, David Heinemeier Hansson).
#pragma once

#include "filepicker.h"

#include <QVariantMap>

class QDBusPendingCallWatcher;

// Opens files through the xdg-desktop-portal FileChooser, so the app never
// draws its own file dialog and works inside sandboxes.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openAudio() override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    void requestFile(const QString &method, const QString &title, const QVariantMap &options);
    bool connectToRequestPath(const QString &path);
    void clearPending();
    void closePendingRequest(); // best-effort Close of a wedged request

    QString m_pendingPath;
    bool m_pending = false;
    // Bumped per request, so a late reply for a replaced request is ignored.
    int m_pendingGeneration = 0;
    QDBusPendingCallWatcher *m_pendingWatcher = nullptr;
};
