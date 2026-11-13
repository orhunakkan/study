#pragma once

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "filetree.h"
#include "tabs.h"

#include "bridge.h"
class FilePicker;

// Study's state: the opened folder, the tabs, and everything that touches the
// disk (reading, saving, watching, the session). The document page renders
// and edits; it reaches this object through Bridge.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString rootFolder READ rootFolder NOTIFY rootChanged)
    Q_PROPERTY(QString rootName READ rootName NOTIFY rootChanged)
    Q_PROPERTY(FileTree *tree READ tree CONSTANT)
    Q_PROPERTY(TabsModel *tabs READ tabs CONSTANT)
    Q_PROPERTY(Bridge *bridge READ bridge CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
    Q_PROPERTY(QString currentRelativePath READ currentRelativePath NOTIFY currentChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY currentChanged)
    Q_PROPERTY(bool currentChangedOnDisk READ currentChangedOnDisk NOTIFY currentChanged)
    Q_PROPERTY(bool currentMissing READ currentMissing NOTIFY currentChanged)
    Q_PROPERTY(bool hasDirty READ hasDirty NOTIFY dirtyChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY dirtyChanged)
    Q_PROPERTY(bool canReopen READ canReopen NOTIFY closedChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    // sessionFile empty: ~/.config/study/session.ini. Tests pass a temp file.
    explicit Backend(const QString &sessionFile = {}, FilePicker *picker = nullptr, QObject *parent = nullptr);
    ~Backend() override;

    QString rootFolder() const { return m_root; }
    QString rootName() const;
    FileTree *tree() const { return m_tree; }
    TabsModel *tabs() const { return m_tabs; }
    Bridge *bridge() const { return m_bridge; }
    int currentIndex() const { return m_current; }
    QString currentPath() const;
    QString currentRelativePath() const;
    bool editing() const;
    bool currentChangedOnDisk() const { return m_current >= 0 && m_tabs->at(m_current).changedOnDisk; }
    bool currentMissing() const { return m_current >= 0 && m_tabs->at(m_current).missing; }
    bool hasDirty() const { return dirtyCount() > 0; }
    int dirtyCount() const;
    bool canReopen() const { return !m_closed.isEmpty(); }
    QString message() const { return m_message; }

    // Paths from the command line or a second launch: folders open in the
    // sidebar, Markdown files open as tabs.
    Q_INVOKABLE void openArguments(const QStringList &arguments);
    Q_INVOKABLE void openFolder(const QString &path);
    Q_INVOKABLE void chooseFolder();
    Q_INVOKABLE bool openFile(const QString &path, bool transient = false, const QString &fragment = {});
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void cycle(int delta);
    Q_INVOKABLE void makePermanent(int index);
    Q_INVOKABLE void moveTab(int from, int to);

    // Closes a clean tab; a tab with unsaved edits emits confirmClose instead.
    Q_INVOKABLE void requestClose(int index);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void saveAndClose(int index);
    Q_INVOKABLE void reopenClosed();

    Q_INVOKABLE void save(int index);
    Q_INVOKABLE void saveCurrent() { save(m_current); }
    // Saves every edited tab, then emits allSaved (and quitReady if asked).
    Q_INVOKABLE void saveAll(bool thenQuit);
    Q_INVOKABLE void discardAllAndQuit();

    Q_INVOKABLE void toggleMode();
    Q_INVOKABLE void reloadFromDisk(int index);
    Q_INVOKABLE void keepMine(int index);
    Q_INVOKABLE void clearMessage();

    // Markdown files under the folder, relative to it, for Go to File.
    Q_INVOKABLE QStringList markdownFiles() const;
    Q_INVOKABLE QString absolutePath(const QString &relative) const;

    Q_INVOKABLE void saveSession();
    void restoreSession();

    // Called by Bridge for the document page.
    void pageReady();
    void setDirty(int id, bool dirty);
    void writeText(int id, const QString &text);
    void setTopLine(int id, double line);
    void switchMode(int id, bool editing, int line);
    void openLink(const QString &path, const QString &fragment);
    void openExternal(const QString &url);

    static QStringList scanMarkdown(const QString &root, int limit = 20000);

signals:
    void rootChanged();
    void currentChanged();
    void dirtyChanged();
    void closedChanged();
    void messageChanged();
    void confirmClose(int index);
    void allSaved();
    void quitReady();
    void raiseRequested();

private:
    void setCurrent(int index);
    void announce(const Tab &tab, const QString &fragment = {});
    void closeAt(int index);
    void setMessage(const QString &message);
    void scheduleSession();
    void checkDisk();
    void watch(const QString &path);
    void unwatch(const QString &path);
    void finishSaveAll();
    bool readFile(const QString &path, QString *text, QByteArray *hash);

    FileTree *m_tree;
    TabsModel *m_tabs;
    Bridge *m_bridge;
    FilePicker *m_picker;
    QString m_sessionFile;
    QString m_root;
    int m_current = -1;
    int m_nextId = 0;
    bool m_pageReady = false;
    bool m_restoring = false;
    QString m_message;
    QStringList m_closed;               // recently closed paths, newest last
    QSet<int> m_closeAfterSave;
    QSet<int> m_pendingSaves;
    bool m_quitAfterSave = false;
    bool m_savingAll = false;
    QHash<int, QElapsedTimer> m_lastToggle;
    QFileSystemWatcher m_watcher;
    QSet<QString> m_changedPaths;
    QTimer m_diskTimer;
    QTimer m_sessionTimer;
    QTimer m_messageTimer;
};
