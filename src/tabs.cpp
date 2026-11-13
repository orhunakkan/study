#include "tabs.h"

#include <QDir>
#include <QFileInfo>

int TabsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_tabs.size();
}

QVariant TabsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_tabs.size())
        return {};
    const Tab &tab = m_tabs[index.row()];
    switch (role) {
    case IdRole: return tab.id;
    case PathRole: return tab.path;
    case TitleRole: return title(index.row());
    case TransientRole: return tab.transient;
    case DirtyRole: return tab.dirty;
    case EditingRole: return tab.editing;
    case ChangedOnDiskRole: return tab.changedOnDisk;
    case MissingRole: return tab.missing;
    }
    return {};
}

QHash<int, QByteArray> TabsModel::roleNames() const {
    return {
        {IdRole, "tabId"}, {PathRole, "path"}, {TitleRole, "title"}, {TransientRole, "peek"},
        {DirtyRole, "dirty"}, {EditingRole, "editing"}, {ChangedOnDiskRole, "changedOnDisk"},
        {MissingRole, "missing"},
    };
}

int TabsModel::indexOfId(int id) const {
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs[i].id == id)
            return i;
    return -1;
}

int TabsModel::indexOfPath(const QString &path) const {
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs[i].path == path)
            return i;
    return -1;
}

void TabsModel::insert(int index, const Tab &tab) {
    index = qBound(0, index, int(m_tabs.size()));
    beginInsertRows({}, index, index);
    m_tabs.insert(index, tab);
    endInsertRows();
    emit countChanged();
    // Titles depend on the other tabs' names.
    if (m_tabs.size() > 1)
        emit dataChanged(this->index(0), this->index(m_tabs.size() - 1), {TitleRole});
}

Tab TabsModel::take(int index) {
    beginRemoveRows({}, index, index);
    Tab tab = m_tabs.takeAt(index);
    endRemoveRows();
    emit countChanged();
    if (!m_tabs.isEmpty())
        emit dataChanged(this->index(0), this->index(m_tabs.size() - 1), {TitleRole});
    return tab;
}

void TabsModel::move(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= m_tabs.size() || to >= m_tabs.size())
        return;
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_tabs.move(from, to);
    endMoveRows();
}

void TabsModel::changed(int index) {
    if (index >= 0 && index < m_tabs.size())
        emit dataChanged(this->index(index), this->index(index));
}

QString TabsModel::title(int index) const {
    const QFileInfo info(m_tabs[index].path);
    const QString name = info.fileName();
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (i != index && QFileInfo(m_tabs[i].path).fileName() == name)
            return QStringLiteral("%1 — %2").arg(name, info.dir().dirName());
    }
    return name;
}
