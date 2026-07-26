#pragma once

#include "domain/Movie.h"

#include <QAbstractListModel>
#include <QList>

namespace xyz {

// Role-based list model exposing Movie data to QListView (icon mode).
// Roles provide the fields the cover-grid delegate paints: title, year,
// cover path, director, badges. Heavy fields (cast, audio, ...) are
// fetched on demand when the user selects a movie.
class MovieListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        OriginalTitleRole,
        SortTitleRole,
        YearRole,
        RuntimeRole,
        FormatRole,
        CoverFrontPathRole,
        CoverBackPathRole,
        DirectorNameRole,        // first credit with creditType == "Direction"
        RatingValueRole,
        RatingAgeRole,
        ReviewFilmRole,          // user's own film rating (0-10); matches tree Rating sort
        ReviewVideoRole,
        ReviewAudioRole,
        ReviewExtrasRole,
        GenresJoinedRole,        // ", "-joined for label rendering
        StudiosJoinedRole,
        CaseTypeRole,
        AspectRatioRole,
        TmdbIdRole,
        PurchaseDateRole,        // QDate; matches tree PurchaseDate column sort
        BoxSetParentIdRole,
        IsLoanedRole,
        IsBoxSetParentRole,
        MembershipTypeRole,      // DP4 <CollectionType> body: "Owned", "Wishlist", …
        MembershipIsOwnedRole    // its IsPartOfOwnedCollection attribute
    };
    Q_ENUM(Roles)

    explicit MovieListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Bulk-replace the backing list. Cheaper than per-insert when the
    // entire result set changes (e.g. on search refresh).
    void setMovies(QList<Movie> movies);

    // Direct access for the controller — when QML asks for detail by id,
    // we look it up here without going back to the DB.
    const QList<Movie>& movies() const { return m_movies; }

    // Lookup by primary id. Returns nullptr if not found — the caller is
    // expected to be the controller, which lives in the same process and
    // doesn't outlive this model.
    const Movie* find(const QString& id) const;

    int indexOfId(const QString& id) const;

private:
    QList<Movie> m_movies;
};

} // namespace xyz
