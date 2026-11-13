#pragma once

#include <QVariantMap>

#include "filepicker.h"

class QDBusPendingCallWatcher;

// The folder chooser through xdg-desktop-portal, so it matches the desktop.
// Follows Omacut's portal file picker (MIT, David Heinemeier Hansson).
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void chooseFolder(const QString &startIn) override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    bool connectToRequestPath(const QString &path);
    void clearPending();
    void closePendingRequest();

    QString m_pendingPath;
    bool m_pending = false;
    // Bumped per request, so a late reply for a replaced request is ignored.
    int m_pendingGeneration = 0;
    QDBusPendingCallWatcher *m_pendingWatcher = nullptr;
};
