#include "CollectionFilterProxyModel.h"

namespace xyz {

CollectionFilterProxyModel::CollectionFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void CollectionFilterProxyModel::setRoles(int membershipTypeRole,
                                          int membershipIsOwnedRole,
                                          int isBoxSetParentRole)
{
    m_typeRole         = membershipTypeRole;
    m_ownedRole        = membershipIsOwnedRole;
    m_boxSetParentRole = isBoxSetParentRole;
    invalidateRowsFilter();
}

void CollectionFilterProxyModel::setStatus(CollectionStatus status)
{
    if (m_status == status) return;
    m_status = status;
    invalidateRowsFilter();
}

void CollectionFilterProxyModel::setHideBoxSetParents(bool hide)
{
    if (m_hideBoxSetParents == hide) return;
    m_hideBoxSetParents = hide;
    invalidateRowsFilter();
}

bool CollectionFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                  const QModelIndex& sourceParent) const
{
    const QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex index = src->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) return true;

    if (m_hideBoxSetParents && m_boxSetParentRole >= 0
        && index.data(m_boxSetParentRole).toBool())
        return false;

    if (m_status == CollectionStatus::All || m_typeRole < 0)
        return true;

    CollectionMembership membership;
    membership.type = index.data(m_typeRole).toString();
    membership.isPartOfOwnedCollection =
        (m_ownedRole >= 0) ? index.data(m_ownedRole).toBool() : true;
    return matchesStatus(membership, m_status);
}

} // namespace xyz
