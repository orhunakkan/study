// Follows Omacut's portal file picker (MIT, David Heinemeier Hansson).
#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QRandomGenerator>

namespace {
QString portalToken() {
    return QStringLiteral("study_%1").arg(QRandomGenerator::global()->generate());
}

// The portal takes paths as NUL-terminated byte strings.
QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {}

void PortalFilePicker::chooseFolder(const QString &startIn) {
    // A request whose Response never arrives (the portal crashed with the
    // dialog up) must not wedge every later dialog: close it and move on.
    if (m_pending) {
        closePendingRequest();
        clearPending();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"), bus);
    if (!portal.isValid()) {
        emit failed(tr("The XDG desktop portal file chooser is not available."));
        return;
    }

    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), tr("Open Folder"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("directory"), true);
    const QString start = QDir(startIn).exists() ? startIn : QDir::homePath();
    options.insert(QStringLiteral("current_folder"), portalPathBytes(start));

    // Subscribe at the request path the portal derives from our handle_token
    // before calling, so the response can't slip past.
    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString predicted = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    m_pending = true;
    if (!connectToRequestPath(predicted)) {
        clearPending();
        emit failed(tr("Could not listen for the folder chooser's answer."));
        return;
    }

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(QStringLiteral("OpenFile"), QString(), tr("Open Folder"), options), this);
    m_pendingWatcher = watcher;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;
        if (generation != m_pendingGeneration || !m_pending)
            return;
        if (reply.isError()) {
            clearPending();
            emit failed(tr("The folder chooser failed: %1").arg(reply.error().message()));
            return;
        }
        // Old portals can hand back another request path; follow it.
        const QString actual = reply.value().path();
        if (actual != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actual)) {
                clearPending();
                emit failed(tr("Could not listen for the folder chooser's answer."));
            }
        }
    });
}

bool PortalFilePicker::connectToRequestPath(const QString &path) {
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint,QVariantMap)));
}

void PortalFilePicker::closePendingRequest() {
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                           QStringLiteral("org.freedesktop.portal.Request"), QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    clearPending();
    if (response != 0)
        return;
    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (!uris.isEmpty())
        emit folderSelected(QUrl(uris.first()));
}

void PortalFilePicker::clearPending() {
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
    }
    m_pendingPath.clear();
    m_pending = false;
}
