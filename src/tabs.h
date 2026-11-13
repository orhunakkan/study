#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QList>

struct Tab {
    int id = 0;
    QString path;
    bool transient = false;      // a single-click "peek", replaced by the next one
    bool dirty = false;
    bool editing = false;        // false: rendered preview (the default)
    double topLine = 0;          // source line at the top of the view
    bool changedOnDisk = false;  // changed outside Study while it had edits
    bool missing = false;        // deleted or moved away on disk
    QByteArray diskHash;         // what Study last read or wrote
};

// The open tabs, in strip order, for the tab strip.
class TabsModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        TransientRole,
        DirtyRole,
        EditingRole,
        ChangedOnDiskRole,
        MissingRole,
    };

    explicit TabsModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_tabs.size(); }
    const QList<Tab> &tabs() const { return m_tabs; }
    Tab &at(int index) { return m_tabs[index]; }
    const Tab &at(int index) const { return m_tabs[index]; }
    int indexOfId(int id) const;
    int indexOfPath(const QString &path) const;

    void insert(int index, const Tab &tab);
    Tab take(int index);
    void move(int from, int to);
    void changed(int index);

    // The name shown on a tab, with the parent folder added when another tab
    // has the same file name (two README.md tabs stay distinguishable).
    QString title(int index) const;

signals:
    void countChanged();

private:
    QList<Tab> m_tabs;
};
