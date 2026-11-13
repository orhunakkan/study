#pragma once

#include <QLocalServer>
#include <QObject>
#include <QStringList>

// One Study window per user: a second launch (double-clicking another .md
// file) hands its paths to the running window and exits.
class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(const QString &name = {}, QObject *parent = nullptr);

    // True when another instance took the arguments; this process should exit.
    bool forward(const QStringList &arguments);
    // Starts listening for later launches.
    bool listen();

signals:
    void received(const QStringList &arguments);

private:
    QString m_name;
    QLocalServer m_server;
};
