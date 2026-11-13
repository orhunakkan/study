#include "backend.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include "bridge.h"
#include "filepicker.h"

namespace {
QByteArray hashOf(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
}

QString cleanAbsolute(const QString &path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

constexpr int kClosedHistory = 20;
}

Backend::Backend(const QString &sessionFile, FilePicker *picker, QObject *parent)
    : QObject(parent),
      m_tree(new FileTree(this)),
      m_tabs(new TabsModel(this)),
      m_bridge(new Bridge(this)),
      m_picker(picker),
      m_sessionFile(sessionFile.isEmpty()
                        ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/study/session.ini"
                        : sessionFile) {
    // Disk events come in bursts (an editor's atomic save is a delete and a
    // create), so look once things settle.
    m_diskTimer.setSingleShot(true);
    m_diskTimer.setInterval(120);
    connect(&m_diskTimer, &QTimer::timeout, this, &Backend::checkDisk);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        m_changedPaths.insert(path);
        m_diskTimer.start();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &dir) {
        // A file replaced or recreated inside a watched folder.
        for (const Tab &tab : m_tabs->tabs())
            if (QFileInfo(tab.path).absolutePath() == dir)
                m_changedPaths.insert(tab.path);
        m_diskTimer.start();
    });

    m_sessionTimer.setSingleShot(true);
    m_sessionTimer.setInterval(400);
    connect(&m_sessionTimer, &QTimer::timeout, this, &Backend::saveSession);

    m_messageTimer.setSingleShot(true);
    m_messageTimer.setInterval(5000);
    connect(&m_messageTimer, &QTimer::timeout, this, &Backend::clearMessage);

    if (m_picker) {
        connect(m_picker, &FilePicker::folderSelected, this, [this](const QUrl &url) {
            if (url.isLocalFile())
                openFolder(url.toLocalFile());
        });
        connect(m_picker, &FilePicker::failed, this, &Backend::setMessage);
    }
}

Backend::~Backend() = default;

QString Backend::rootName() const {
    return m_root.isEmpty() ? QString() : QFileInfo(m_root).fileName();
}

QString Backend::currentPath() const {
    return m_current >= 0 ? m_tabs->at(m_current).path : QString();
}

QString Backend::currentRelativePath() const {
    const QString path = currentPath();
    if (path.isEmpty() || m_root.isEmpty() || !path.startsWith(m_root + '/'))
        return path;
    return path.mid(m_root.size() + 1);
}

bool Backend::editing() const {
    return m_current >= 0 && m_tabs->at(m_current).editing;
}

int Backend::dirtyCount() const {
    int n = 0;
    for (const Tab &tab : m_tabs->tabs())
        n += tab.dirty ? 1 : 0;
    return n;
}

void Backend::setMessage(const QString &message) {
    m_message = message;
    emit messageChanged();
    if (!message.isEmpty())
        m_messageTimer.start();
}

void Backend::clearMessage() {
    if (m_message.isEmpty())
        return;
    m_message.clear();
    emit messageChanged();
}

// ---------------------------------------------------------------------------
// Opening

void Backend::openArguments(const QStringList &arguments) {
    emit raiseRequested();
    QStringList files;
    for (const QString &argument : arguments) {
        if (argument.isEmpty())
            continue;
        const QUrl url(argument);
        const QString path = cleanAbsolute(url.isLocalFile() ? url.toLocalFile() : argument);
        const QFileInfo info(path);
        if (info.isDir())
            openFolder(path);
        else if (info.exists())
            files << path;
        else
            setMessage(tr("%1 doesn't exist.").arg(path));
    }
    for (const QString &file : files) {
        // A file on its own brings its folder along when nothing is open yet.
        if (m_root.isEmpty())
            openFolder(QFileInfo(file).absolutePath());
        openFile(file, false);
    }
}

void Backend::openFolder(const QString &path) {
    const QString clean = cleanAbsolute(path);
    if (!QFileInfo(clean).isDir()) {
        setMessage(tr("%1 isn't a folder.").arg(clean));
        return;
    }
    if (clean == m_root)
        return;
    m_root = clean;
    m_tree->setRoot(clean);
    emit rootChanged();
    scheduleSession();
}

void Backend::chooseFolder() {
    if (m_picker)
        m_picker->chooseFolder(m_root.isEmpty() ? QDir::homePath() : m_root);
}

bool Backend::readFile(const QString &path, QString *text, QByteArray *hash) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray bytes = file.readAll();
    if (text)
        *text = QString::fromUtf8(bytes);
    if (hash)
        *hash = hashOf(bytes);
    return true;
}

bool Backend::openFile(const QString &path, bool transient, const QString &fragment) {
    const QString clean = cleanAbsolute(path);
    if (!FileTree::isMarkdown(clean)) {
        setMessage(tr("Study opens Markdown files only."));
        return false;
    }

    const int existing = m_tabs->indexOfPath(clean);
    if (existing >= 0) {
        if (!transient)
            makePermanent(existing);
        setCurrent(existing);
        if (!fragment.isEmpty() && m_pageReady)
            emit m_bridge->fragmentRequested(m_tabs->at(existing).id, fragment);
        return true;
    }

    Tab tab;
    QString text;
    if (!readFile(clean, &text, &tab.diskHash)) {
        setMessage(tr("Couldn't read %1.").arg(clean));
        return false;
    }
    tab.id = ++m_nextId;
    tab.path = clean;
    tab.transient = transient;

    // Like Sublime Text, one peek tab at a time: a new peek replaces the old.
    int at = m_current >= 0 ? m_current + 1 : m_tabs->count();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->at(i).transient && transient) {
            at = i;
            closeAt(i);
            break;
        }
    }
    at = qMin(at, m_tabs->count());
    m_tabs->insert(at, tab);
    watch(clean);
    if (m_pageReady)
        emit m_bridge->documentOpened(tab.id, tab.path, QFileInfo(tab.path).absolutePath(), m_root, text,
                                      "preview", 0, fragment);
    m_current = -1;
    setCurrent(at);
    scheduleSession();
    return true;
}

void Backend::announce(const Tab &tab, const QString &fragment) {
    QString text;
    readFile(tab.path, &text, nullptr);
    emit m_bridge->documentOpened(tab.id, tab.path, QFileInfo(tab.path).absolutePath(), m_root, text,
                                  tab.editing ? "edit" : "preview", tab.topLine, fragment);
}

void Backend::setCurrent(int index) {
    if (index >= m_tabs->count())
        index = m_tabs->count() - 1;
    if (index == m_current)
        return;
    m_current = index;
    emit currentChanged();
    if (m_pageReady)
        emit m_bridge->activated(index >= 0 ? m_tabs->at(index).id : -1);
    scheduleSession();
}

void Backend::activate(int index) {
    if (index >= 0 && index < m_tabs->count())
        setCurrent(index);
}

void Backend::cycle(int delta) {
    const int n = m_tabs->count();
    if (n > 0)
        setCurrent(((m_current + delta) % n + n) % n);
}

void Backend::makePermanent(int index) {
    if (index < 0 || index >= m_tabs->count() || !m_tabs->at(index).transient)
        return;
    m_tabs->at(index).transient = false;
    m_tabs->changed(index);
    scheduleSession();
}

void Backend::moveTab(int from, int to) {
    if (from < 0 || to < 0 || from >= m_tabs->count() || to >= m_tabs->count() || from == to)
        return;
    const int currentId = m_current >= 0 ? m_tabs->at(m_current).id : -1;
    m_tabs->move(from, to);
    m_current = m_tabs->indexOfId(currentId);
    emit currentChanged();
    scheduleSession();
}

// ---------------------------------------------------------------------------
// Closing

void Backend::requestClose(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    if (m_tabs->at(index).dirty)
        emit confirmClose(index);
    else
        closeTab(index);
}

void Backend::closeTab(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    const bool wasDirty = m_tabs->at(index).dirty;
    m_closed.removeAll(m_tabs->at(index).path);
    m_closed.append(m_tabs->at(index).path);
    while (m_closed.size() > kClosedHistory)
        m_closed.removeFirst();
    emit closedChanged();
    closeAt(index);
    if (wasDirty)
        emit dirtyChanged();
    scheduleSession();
}

void Backend::closeAt(int index) {
    const int currentId = m_current >= 0 ? m_tabs->at(m_current).id : -1;
    const Tab tab = m_tabs->take(index);
    unwatch(tab.path);
    m_closeAfterSave.remove(tab.id);
    m_pendingSaves.remove(tab.id);
    if (m_pageReady)
        emit m_bridge->documentClosed(tab.id);

    if (tab.id == currentId) {
        // The neighbour to the right takes its place, else the one to the left.
        m_current = -1;
        setCurrent(qMin(index, m_tabs->count() - 1));
    } else {
        m_current = m_tabs->indexOfId(currentId);
        emit currentChanged();
    }
}

void Backend::saveAndClose(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    m_closeAfterSave.insert(m_tabs->at(index).id);
    save(index);
}

void Backend::reopenClosed() {
    while (!m_closed.isEmpty()) {
        const QString path = m_closed.takeLast();
        emit closedChanged();
        if (QFileInfo::exists(path) && openFile(path, false))
            return;
    }
}

// ---------------------------------------------------------------------------
// Saving

void Backend::save(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    const Tab &tab = m_tabs->at(index);
    m_pendingSaves.insert(tab.id);
    // The page holds the edited text; it answers with saveText.
    if (m_pageReady)
        emit m_bridge->textRequested(tab.id);
}

void Backend::writeText(int id, const QString &text) {
    const int index = m_tabs->indexOfId(id);
    if (index < 0)
        return;
    m_pendingSaves.remove(id);
    Tab &tab = m_tabs->at(index);
    const QByteArray bytes = text.toUtf8();

    // Write beside the file and rename over it, so a crash never leaves half
    // a document behind.
    QSaveFile file(tab.path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setMessage(tr("Couldn't save %1: %2").arg(QFileInfo(tab.path).fileName(), file.errorString()));
        m_closeAfterSave.remove(id);
        if (m_savingAll) {
            m_savingAll = false;
            m_quitAfterSave = false;
        }
        return;
    }
    tab.diskHash = hashOf(bytes);
    tab.changedOnDisk = false;
    tab.missing = false;
    tab.transient = false;
    const bool wasDirty = tab.dirty;
    tab.dirty = false;
    m_tabs->changed(index);
    watch(tab.path);
    emit m_bridge->saved(id);
    if (wasDirty)
        emit dirtyChanged();

    if (m_closeAfterSave.remove(id))
        closeTab(m_tabs->indexOfId(id));
    if (m_savingAll && m_pendingSaves.isEmpty())
        finishSaveAll();
}

void Backend::saveAll(bool thenQuit) {
    m_quitAfterSave = thenQuit;
    m_savingAll = true;
    for (int i = 0; i < m_tabs->count(); ++i)
        if (m_tabs->at(i).dirty)
            save(i);
    if (m_pendingSaves.isEmpty())
        finishSaveAll();
}

void Backend::finishSaveAll() {
    m_savingAll = false;
    emit allSaved();
    if (m_quitAfterSave && !hasDirty()) {
        m_quitAfterSave = false;
        saveSession();
        emit quitReady();
    }
}

void Backend::discardAllAndQuit() {
    saveSession();
    emit quitReady();
}

// ---------------------------------------------------------------------------
// Modes and page callbacks

void Backend::toggleMode() {
    if (m_current < 0)
        return;
    switchMode(m_tabs->at(m_current).id, !m_tabs->at(m_current).editing, -1);
}

void Backend::switchMode(int id, bool editing, int line) {
    const int index = m_tabs->indexOfId(id);
    if (index < 0)
        return;
    Tab &tab = m_tabs->at(index);
    if (tab.editing == editing && line < 0)
        return;
    // Ctrl+Shift+V can arrive twice (the window's shortcut and the editor's
    // own key binding); the second within a moment is the same keypress.
    QElapsedTimer &last = m_lastToggle[id];
    if (line < 0 && last.isValid() && last.elapsed() < 200)
        return;
    last.start();
    tab.editing = editing;
    if (editing)
        tab.transient = false;   // editing is committing to the tab
    m_tabs->changed(index);
    if (index == m_current)
        emit currentChanged();
    if (m_pageReady)
        emit m_bridge->modeRequested(id, editing ? "edit" : "preview", line);
    scheduleSession();
}

void Backend::pageReady() {
    // The page (re)loaded: hand it every open document, then the current one.
    m_pageReady = true;
    for (int i = 0; i < m_tabs->count(); ++i) {
        Tab &tab = m_tabs->at(i);
        if (tab.dirty) {
            tab.dirty = false;   // a reloaded page starts from the file again
            m_tabs->changed(i);
        }
        announce(tab);
    }
    emit dirtyChanged();
    emit m_bridge->activated(m_current >= 0 ? m_tabs->at(m_current).id : -1);
}

void Backend::setDirty(int id, bool dirty) {
    const int index = m_tabs->indexOfId(id);
    if (index < 0 || m_tabs->at(index).dirty == dirty)
        return;
    Tab &tab = m_tabs->at(index);
    tab.dirty = dirty;
    if (dirty)
        tab.transient = false;
    m_tabs->changed(index);
    emit dirtyChanged();
}

void Backend::setTopLine(int id, double line) {
    const int index = m_tabs->indexOfId(id);
    if (index < 0)
        return;
    m_tabs->at(index).topLine = line;
    scheduleSession();
}

void Backend::openLink(const QString &path, const QString &fragment) {
    const QFileInfo info(path);
    if (FileTree::isMarkdown(path)) {
        if (info.exists())
            openFile(path, false, fragment);
        else
            setMessage(tr("%1 doesn't exist.").arg(path));
        return;
    }
    if (info.isDir()) {
        setMessage(tr("%1 is a folder.").arg(info.fileName()));
        return;
    }
    if (!info.exists()) {
        setMessage(tr("%1 doesn't exist.").arg(path));
        return;
    }
    // Anything else goes to the app the desktop uses for it.
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void Backend::openExternal(const QString &url) {
    const QUrl parsed(url);
    static const QStringList schemes = {"http", "https", "mailto", "ftp"};
    if (schemes.contains(parsed.scheme().toLower()))
        QDesktopServices::openUrl(parsed);
}

// ---------------------------------------------------------------------------
// Disk changes

void Backend::watch(const QString &path) {
    if (!m_watcher.files().contains(path) && QFileInfo::exists(path))
        m_watcher.addPath(path);
    const QString dir = QFileInfo(path).absolutePath();
    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);
}

void Backend::unwatch(const QString &path) {
    if (m_tabs->indexOfPath(path) >= 0)
        return;
    m_watcher.removePath(path);
    const QString dir = QFileInfo(path).absolutePath();
    for (const Tab &tab : m_tabs->tabs())
        if (QFileInfo(tab.path).absolutePath() == dir)
            return;
    m_watcher.removePath(dir);
}

void Backend::checkDisk() {
    const QSet<QString> paths = std::exchange(m_changedPaths, {});
    for (const QString &path : paths) {
        const int index = m_tabs->indexOfPath(path);
        if (index < 0)
            continue;
        Tab &tab = m_tabs->at(index);
        if (!QFileInfo::exists(path)) {
            if (!tab.missing) {
                tab.missing = true;
                m_tabs->changed(index);
                if (index == m_current)
                    emit currentChanged();
            }
            continue;
        }
        // Atomic saves replace the file, which drops it from the watch.
        if (!m_watcher.files().contains(path))
            m_watcher.addPath(path);
        QString text;
        QByteArray hash;
        if (!readFile(path, &text, &hash) || hash == tab.diskHash) {
            if (tab.missing) {
                tab.missing = false;
                m_tabs->changed(index);
            }
            continue;   // our own save, or a touch without a change
        }
        tab.missing = false;
        if (tab.dirty) {
            // Never overwrite edits: ask in the window instead.
            tab.changedOnDisk = true;
        } else {
            tab.diskHash = hash;
            tab.changedOnDisk = false;
            if (m_pageReady)
                emit m_bridge->documentReloaded(tab.id, text);
        }
        m_tabs->changed(index);
        if (index == m_current)
            emit currentChanged();
    }
}

void Backend::reloadFromDisk(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    Tab &tab = m_tabs->at(index);
    QString text;
    if (!readFile(tab.path, &text, &tab.diskHash)) {
        setMessage(tr("Couldn't read %1.").arg(tab.path));
        return;
    }
    const bool wasDirty = tab.dirty;
    tab.changedOnDisk = false;
    tab.dirty = false;
    tab.missing = false;
    m_tabs->changed(index);
    if (m_pageReady)
        emit m_bridge->documentReloaded(tab.id, text);
    if (wasDirty)
        emit dirtyChanged();
    if (index == m_current)
        emit currentChanged();
}

void Backend::keepMine(int index) {
    if (index < 0 || index >= m_tabs->count())
        return;
    Tab &tab = m_tabs->at(index);
    tab.changedOnDisk = false;
    // The next save overwrites what's on disk; until then, newer disk
    // changes still count as changes.
    readFile(tab.path, nullptr, &tab.diskHash);
    m_tabs->changed(index);
    if (index == m_current)
        emit currentChanged();
}

// ---------------------------------------------------------------------------
// Go to File

QStringList Backend::scanMarkdown(const QString &root, int limit) {
    QStringList found;
    if (root.isEmpty())
        return found;
    // Walk by hand so excluded folders (and dependency trees full of READMEs)
    // are never entered.
    QStringList pending{root};
    while (!pending.isEmpty() && found.size() < limit) {
        const QString dir = pending.takeFirst();
        const auto entries = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                                     QDir::Name | QDir::DirsLast);
        for (const QFileInfo &entry : entries) {
            if (entry.isDir()) {
                if (!entry.isSymLink() && !FileTree::isExcluded(entry.fileName(), true) &&
                    entry.fileName() != "node_modules")
                    pending << entry.absoluteFilePath();
            } else if (FileTree::isMarkdown(entry.fileName())) {
                found << entry.absoluteFilePath().mid(root.size() + 1);
                if (found.size() >= limit)
                    break;
            }
        }
    }
    found.sort(Qt::CaseInsensitive);
    return found;
}

QStringList Backend::markdownFiles() const {
    return scanMarkdown(m_root);
}

QString Backend::absolutePath(const QString &relative) const {
    return QDir(m_root).filePath(relative);
}

// ---------------------------------------------------------------------------
// Session

void Backend::scheduleSession() {
    if (!m_restoring)
        m_sessionTimer.start();
}

void Backend::saveSession() {
    m_sessionTimer.stop();
    QSettings settings(m_sessionFile, QSettings::IniFormat);
    settings.setValue("root", m_root);
    settings.beginWriteArray("tabs");
    int n = 0;
    for (const Tab &tab : m_tabs->tabs()) {
        if (tab.transient)
            continue;   // peeks aren't worth restoring
        settings.setArrayIndex(n++);
        settings.setValue("path", tab.path);
        settings.setValue("editing", tab.editing);
        settings.setValue("topLine", tab.topLine);
    }
    settings.endArray();
    settings.setValue("current", currentPath());
    settings.setValue("closed", m_closed);
}

void Backend::restoreSession() {
    QSettings settings(m_sessionFile, QSettings::IniFormat);
    m_restoring = true;
    const QString root = settings.value("root").toString();
    if (!root.isEmpty() && QFileInfo(root).isDir())
        openFolder(root);
    const int count = settings.beginReadArray("tabs");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString path = settings.value("path").toString();
        if (!QFileInfo::exists(path) || !FileTree::isMarkdown(path) || m_tabs->indexOfPath(path) >= 0)
            continue;
        Tab tab;
        QString text;
        if (!readFile(path, &text, &tab.diskHash))
            continue;
        tab.id = ++m_nextId;
        tab.path = cleanAbsolute(path);
        tab.editing = settings.value("editing").toBool();
        tab.topLine = settings.value("topLine").toDouble();
        m_tabs->insert(m_tabs->count(), tab);
        watch(tab.path);
    }
    settings.endArray();
    m_closed = settings.value("closed").toStringList();
    emit closedChanged();
    const int current = m_tabs->indexOfPath(settings.value("current").toString());
    m_current = -1;
    setCurrent(current >= 0 ? current : m_tabs->count() - 1);
    m_restoring = false;
}
