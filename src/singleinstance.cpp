#include "singleinstance.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <unistd.h>

SingleInstance::SingleInstance(const QString &name, QObject *parent)
    : QObject(parent), m_name(name.isEmpty() ? QStringLiteral("study-%1").arg(getuid()) : name) {
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                if (!socket->canReadLine())
                    return;
                const auto array = QJsonDocument::fromJson(socket->readLine()).array();
                QStringList arguments;
                for (const auto &value : array)
                    arguments << value.toString();
                emit received(arguments);
                socket->disconnectFromServer();
            });
        }
    });
}

bool SingleInstance::forward(const QStringList &arguments) {
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(300))
        return false;
    // Relative paths mean this process's directory, not the running one's.
    QJsonArray array;
    for (const QString &argument : arguments)
        array.append(argument.contains("://") ? argument : QFileInfo(argument).absoluteFilePath());
    socket.write(QJsonDocument(array).toJson(QJsonDocument::Compact) + "\n");
    socket.waitForBytesWritten(1000);
    socket.waitForDisconnected(1000);
    return true;
}

bool SingleInstance::listen() {
    QLocalServer::removeServer(m_name);   // a crashed instance's stale socket
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    return m_server.listen(m_name);
}
