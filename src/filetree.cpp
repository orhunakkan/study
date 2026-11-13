#include "filetree.h"

#include <QCollator>
#include <QFileSystemModel>
#include <QRegularExpression>

namespace {
// Sublime Text's default folder_exclude_patterns and file_exclude_patterns.
const QStringList kExcludedFolders = {".git", ".svn", ".hg", "CVS", ".Trash", ".Trash-*"};
const QStringList kExcludedFiles = {
    "*.pyc", "*.pyo", "*.exe", "*.dll", "*.obj", "*.o", "*.a", "*.lib", "*.so", "*.dylib",
    "*.ncb", "*.sdf", "*.suo", "*.pdb", "*.idb", ".DS_Store", ".directory", "desktop.ini",
    "*.class", "*.psd", "*.db", "*.sublime-workspace",
};

bool matchesAny(const QString &name, const QStringList &patterns) {
    for (const auto &pattern : patterns) {
        const auto re = QRegularExpression::fromWildcard(pattern, Qt::CaseSensitive);
        if (re.match(name).hasMatch())
            return true;
    }
    return false;
}
}

FileTree::FileTree(QObject *parent) : QSortFilterProxyModel(parent), m_fs(new QFileSystemModel(this)) {
    m_fs->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::AllDirs);
    m_fs->setReadOnly(true);
    m_fs->setResolveSymlinks(true);
    setSourceModel(m_fs);
    setRecursiveFilteringEnabled(false);
    setDynamicSortFilter(true);
    sort(0);
}

void FileTree::setRoot(const QString &path) {
    if (path == m_root)
        return;
    m_root = path;
    m_fs->setRootPath(path);
    emit rootChanged();
}

QModelIndex FileTree::rootIndex() const {
    return m_root.isEmpty() ? QModelIndex() : mapFromSource(m_fs->index(m_root));
}

QModelIndex FileTree::indexForPath(const QString &path) const {
    return mapFromSource(m_fs->index(path));
}

bool FileTree::isMarkdown(const QString &path) {
    static const QStringList suffixes = {"md", "markdown", "mdown", "mkd", "mkdn", "mdwn", "mdtxt", "mdtext"};
    const int dot = path.lastIndexOf('.');
    return dot > path.lastIndexOf('/') && suffixes.contains(path.mid(dot + 1).toLower());
}

bool FileTree::isExcluded(const QString &name, bool isDir) {
    return matchesAny(name, isDir ? kExcludedFolders : kExcludedFiles);
}

QVariant FileTree::data(const QModelIndex &index, int role) const {
    switch (role) {
    case NameRole:
        return QSortFilterProxyModel::data(index, QFileSystemModel::FileNameRole);
    case PathRole:
        return QSortFilterProxyModel::data(index, QFileSystemModel::FilePathRole);
    case IsDirRole:
        return m_fs->isDir(mapToSource(index));
    case IsMarkdownRole: {
        const QModelIndex source = mapToSource(index);
        return !m_fs->isDir(source) && isMarkdown(m_fs->fileName(source));
    }
    default:
        return QSortFilterProxyModel::data(index, role);
    }
}

QHash<int, QByteArray> FileTree::roleNames() const {
    auto names = QSortFilterProxyModel::roleNames();
    names.insert(NameRole, "name");
    names.insert(PathRole, "path");
    names.insert(IsDirRole, "isDir");
    names.insert(IsMarkdownRole, "isMarkdown");
    return names;
}

bool FileTree::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    const QModelIndex index = m_fs->index(sourceRow, 0, sourceParent);
    return !isExcluded(m_fs->fileName(index), m_fs->isDir(index));
}

bool FileTree::filterAcceptsColumn(int sourceColumn, const QModelIndex &) const {
    return sourceColumn == 0;   // a tree of names, not a table of sizes and dates
}

bool FileTree::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    const bool leftDir = m_fs->isDir(left), rightDir = m_fs->isDir(right);
    if (leftDir != rightDir)
        return leftDir;
    static const QCollator collator = [] {
        QCollator c;
        c.setNumericMode(true);
        c.setCaseSensitivity(Qt::CaseInsensitive);
        return c;
    }();
    return collator.compare(m_fs->fileName(left), m_fs->fileName(right)) < 0;
}
