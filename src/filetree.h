#pragma once

#include <QSortFilterProxyModel>

class QFileSystemModel;

// The sidebar's folder tree: every file and folder under the opened folder
// (dotfiles included, like Sublime Text), minus version-control and build
// clutter, folders first. Markdown files are flagged so the sidebar can dim
// everything Study can't open.
class FileTree : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QModelIndex rootIndex READ rootIndex NOTIFY rootChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 100,
        PathRole,
        IsDirRole,
        IsMarkdownRole,
    };

    explicit FileTree(QObject *parent = nullptr);

    void setRoot(const QString &path);
    QModelIndex rootIndex() const;
    Q_INVOKABLE QModelIndex indexForPath(const QString &path) const;

    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    static bool isMarkdown(const QString &path);
    static bool isExcluded(const QString &name, bool isDir);

signals:
    void rootChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QFileSystemModel *m_fs;
    QString m_root;
};
