#pragma once

#include "domain/CollectionMembership.h"

#include <QSortFilterProxyModel>

namespace xyz {

// ---------------------------------------------------------------------------
// CollectionFilterProxyModel — owned / wishlist / all, for both views.
// ---------------------------------------------------------------------------
// The cover grid and the list view sit on different source models
// (MovieListModel vs MovieTreeModel) whose role enums differ, so the role ids
// are injected by the consumer instead of being hard-coded here.
//
// This also replaces the grid's previous "hide box-set parents" filter, which
// was expressed as setFilterRole(IsBoxSetParentRole) plus a "^false$" regular
// expression — that converted a bool to a QString and ran a regex per row.
// ---------------------------------------------------------------------------
class CollectionFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit CollectionFilterProxyModel(QObject* parent = nullptr);

    // Role ids the source model answers. Pass -1 for a role the source does
    // not provide; the corresponding criterion is then skipped.
    void setRoles(int membershipTypeRole, int membershipIsOwnedRole,
                  int isBoxSetParentRole);

    void setStatus(CollectionStatus status);
    CollectionStatus status() const { return m_status; }

    // Cover-grid only: parents are represented there by their child titles.
    void setHideBoxSetParents(bool hide);

protected:
    bool filterAcceptsRow(int sourceRow,
                          const QModelIndex& sourceParent) const override;

private:
    CollectionStatus m_status = CollectionStatus::Owned;
    bool m_hideBoxSetParents  = false;
    int  m_typeRole           = -1;
    int  m_ownedRole          = -1;
    int  m_boxSetParentRole   = -1;
};

} // namespace xyz
