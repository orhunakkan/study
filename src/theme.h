#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

// Follows the Omarchy theme live: the accent for Qt Quick controls, the full
// palette for the sidebar, tabs and the document page, and Hyprland's window
// rounding. Tests construct it with a temp directory and a fake hyprctl.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_colors.value("accent").toString(); }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    bool dark() const { return m_colors.value("mode").toString() != "light"; }
    QVariantMap colors() const { return m_colors; }

    static QVariantMap readColors(const QString &path);
    static QVariantMap defaultColors();

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QVariantMap m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
