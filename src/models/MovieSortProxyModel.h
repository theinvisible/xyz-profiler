#pragma once

#include <QHash>
#include <QSortFilterProxyModel>
#include <QString>

namespace xyz {

// Sort proxy for the role-based MovieListModel (icon view).
// For QTableView, use a plain QSortFilterProxyModel on MovieTableModel.
class MovieSortProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit MovieSortProxyModel(QObject* parent = nullptr);

    QString sortRoleName()  const { return m_sortRoleName; }
    bool    sortDescending() const { return m_descending; }

    void toggleSort(const QString& roleName);
    // Sort explicitly by role + direction (used to mirror the list view's
    // header sort onto the cover grid, and to restore the saved sort on start).
    void sortByRole(const QString& roleName, bool descending);
    void clearSort();

signals:
    void sortChanged();

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    void ensureRoleCache_() const;

    QString m_sortRoleName;
    bool    m_descending = false;
    mutable QHash<QString, int> m_roleIdByName;
};

} // namespace xyz
