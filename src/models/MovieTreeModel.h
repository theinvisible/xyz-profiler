#pragma once

#include "domain/Movie.h"

#include <QAbstractItemModel>
#include <QList>
#include <memory>

namespace xyz {

class MovieTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    // APPEND ONLY. SettingsController persists the visible-column set and the
    // sort column as raw indices, so inserting anywhere but at the end
    // silently reshuffles the user's saved layout.
    enum Column {
        Title = 0, OriginalTitle, SortTitle, Year, Runtime, Format,
        Rating, RatingAge, Director, Genres, Studios, CaseType,
        AspectRatio, TmdbId, PurchaseDate, Loaned, BoxSetParent,
        RatingVideo, RatingAudio, RatingExtras, ColumnCount
    };

    // Column-independent roles for the custom row delegate — return movie-level
    // data regardless of which column index is queried.
    enum Roles {
        CoverPathRole = Qt::UserRole + 100,
        IsLoanedRole,
        ReviewFilmRole,      // user's own film rating, 0-10
        FormatNameRole,      // raw format string ("DVD" / "BluRay" / "UHD")
        IsBoxSetParentRole,
        AgeRole,             // content-rating age (FSK)
        MembershipTypeRole,  // DP4 <CollectionType> body: "Owned", "Wishlist", …
        MembershipIsOwnedRole,
    };

    explicit MovieTreeModel(QObject* parent = nullptr);
    ~MovieTreeModel() override;

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setMovies(const QList<Movie>& movies);
    QString movieIdAtIndex(const QModelIndex& index) const;

private:
    struct Node {
        Movie movie;
        Node* parent = nullptr;
        QList<Node*> children;
        int row = 0;
    };

    Node* nodeFromIndex(const QModelIndex& index) const;
    static QVariant columnData(const Movie& m, int col, int role);
    static QString primaryDirector(const Movie& m);
    void clear();

    Node* m_root = nullptr;
};

} // namespace xyz
