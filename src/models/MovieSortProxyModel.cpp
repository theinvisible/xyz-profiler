#include "MovieSortProxyModel.h"

#include <QDate>
#include <QDateTime>

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

void MovieSortProxyModel::sortByRole(const QString& roleName, bool descending)
{
    ensureRoleCache_();
    auto it = m_roleIdByName.constFind(roleName);
    if (it == m_roleIdByName.constEnd()) return;

    m_sortRoleName = roleName;
    m_descending   = descending;
    setSortRole(it.value());
    invalidate();
    sort(0, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
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

    if (l.typeId() == QMetaType::Int)       return l.toInt() < r.toInt();
    if (l.typeId() == QMetaType::Bool)      return !l.toBool() && r.toBool();
    // Dates compare chronologically, not as formatted strings — otherwise the
    // cover grid sorts e.g. PurchaseDate lexicographically and diverges from
    // the list view.
    if (l.typeId() == QMetaType::QDate)     return l.toDate() < r.toDate();
    if (l.typeId() == QMetaType::QDateTime) return l.toDateTime() < r.toDateTime();
    // Strings: localeAwareCompare to match the list view, whose tree sort proxy
    // has setSortLocaleAware(true) (Qt's default lessThan then compares strings
    // the same way). Using a plain case-insensitive compare here produced a
    // different order than the list view for the same column.
    return l.toString().localeAwareCompare(r.toString()) < 0;
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
