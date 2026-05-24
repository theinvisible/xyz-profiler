#pragma once

#include "domain/Movie.h"

#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

namespace xyz {

// Exposes a list of `Movie` to QML views (CoverGrid, list views, ...).
//
// Roles cover the fields that QML delegates need cheaply at scroll time —
// title, year, format, cover path, runtime, primary director name, rating
// — plus an `id` for navigating to the detail view. Heavy fields (full
// cast/crew, audio tracks, ...) are not exposed as roles; the detail view
// fetches them by id from the controller on demand.
// Not QML_ELEMENT — QML sees this model only via the controller's
// `movies` property, which guarantees it's the live instance backing the
// repository's results.
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
        GenresJoinedRole,        // ", "-joined for label rendering
        IsLoanedRole,
        IsBoxSetParentRole
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

    Q_INVOKABLE int indexOfId(const QString& id) const;

private:
    QList<Movie> m_movies;
};

} // namespace xyz
