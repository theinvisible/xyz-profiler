#pragma once

#include "domain/Movie.h"

#include <QAbstractTableModel>
#include <QFont>
#include <QList>
#include <QString>
#include <QVariant>

namespace xyz {

// Table model for Qt Widgets views (QTableView). Exposes a fixed set of
// columns covering the most useful Movie fields. Each column returns a
// formatted display string (DisplayRole), a raw sortable value (UserRole),
// and optional font / alignment hints.
//
// BoxSetParentIdRole (UserRole + 1) is available on every row so the view
// can indent box-set children.
class MovieTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Title = 0,
        OriginalTitle,
        SortTitle,
        Year,
        Runtime,
        Format,
        Rating,
        RatingAge,
        Director,
        Genres,
        Studios,
        CaseType,
        AspectRatio,
        TmdbId,
        Loaned,
        BoxSetParent,
        ColumnCount
    };

    enum ExtraRoles {
        BoxSetParentIdRole = Qt::UserRole + 1
    };

    explicit MovieTableModel(QObject* parent = nullptr);

    // QAbstractTableModel interface
    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // Bulk-replace the backing list. Resets the model so any attached view
    // re-fetches all data.
    void setMovies(QList<Movie> movies);

    const QList<Movie>& movies() const { return m_movies; }

    // Lookup by primary id. Returns nullptr if the id is not in the list.
    const Movie* find(const QString& id) const;

    // Returns the row index of the movie with the given id, or -1.
    int indexOfId(const QString& id) const;

    // Returns the movie ID at the given row, or an empty string if the
    // row is out of range.
    QString movieIdAtRow(int row) const;

private:
    QList<Movie> m_movies;
};

} // namespace xyz
