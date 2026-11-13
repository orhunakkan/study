#pragma once

#include <QObject>

class Backend;

// What the document page (web/app.js) sees over Qt WebChannel: signals that
// drive it, and slots it calls back. Kept apart from Backend so the page only
// gets this small surface.
class Bridge : public QObject {
    Q_OBJECT

public:
    explicit Bridge(Backend *backend);

public slots:
    void ready();
    void setDirty(int id, bool dirty);
    void saveText(int id, const QString &text);
    void reportTopLine(int id, double line);
    void switchMode(int id, const QString &mode, int line);
    void openPath(const QString &path, const QString &fragment);
    void openExternal(const QString &url);

signals:
    void documentOpened(int id, const QString &path, const QString &dir, const QString &root,
                        const QString &text, const QString &mode, double topLine, const QString &fragment);
    void documentReloaded(int id, const QString &text);
    void documentRenamed(int id, const QString &path, const QString &dir);
    void documentClosed(int id);
    void activated(int id);
    void modeRequested(int id, const QString &mode, int line);
    void textRequested(int id);
    void saved(int id);
    void fragmentRequested(int id, const QString &fragment);

private:
    Backend *m_backend;
};
