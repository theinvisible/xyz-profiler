#include "MovieSortProxyModel.h"

namespace xyz {

MovieSortProxyModel::MovieSortProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void MovieSortProxyModel::toggleSort(const QString& roleName)
{
    ensureRoleCache_();
    auto it = m_roleIdByName.constFind(roleName);
    if (it == m_roleIdByName.constEnd()) return;

    if (m_sortRoleName == roleName) {
        m_descending = !m_descending;
    } else {
        m_sortRoleName = roleName;
        m_descending   = false;
    }
    setSortRole(it.value());
    invalidate();
    sort(0, m_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
    emit sortChanged();
}

void MovieSortProxyModel::clearSort()
{
    m_sortRoleName.clear();
    m_descending = false;
    sort(-1);
    emit sortChanged();
}

bool MovieSortProxyModel::lessThan(const QModelIndex& left,
                                   const QModelIndex& right) const
{
    const QVariant l = sourceModel()->data(left, sortRole());
    const QVariant r = sourceModel()->data(right, sortRole());

    if (l.typeId() == QMetaType::Int)  return l.toInt() < r.toInt();
    if (l.typeId() == QMetaType::Bool) return !l.toBool() && r.toBool();
    return l.toString().compare(r.toString(), Qt::CaseInsensitive) < 0;
}

void MovieSortProxyModel::ensureRoleCache_() const
{
    if (!m_roleIdByName.isEmpty()) return;
    if (!sourceModel()) return;
    const auto names = sourceModel()->roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        m_roleIdByName.insert(QString::fromUtf8(it.value()), it.key());
}

} // namespace xyz
