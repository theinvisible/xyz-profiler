#include "MovieTableModel.h"

#include <QFont>
#include <QVariant>

namespace xyz {
namespace {

// Find the first credit with `creditType == "Direction"` and format as
// "FirstName LastName". Returns an empty string if no director is found.
QString primaryDirector(const Movie& m)
{
    for (const auto& c : m.credits) {
        if (c.creditType.compare(QLatin1String("Direction"), Qt::CaseInsensitive) == 0) {
            QStringList parts;
            if (!c.firstName.isEmpty()) parts << c.firstName;
            if (!c.lastName.isEmpty())  parts << c.lastName;
            return parts.join(QChar(u' '));
        }
    }
    return {};
}

} // namespace

MovieTableModel::MovieTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

int MovieTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return int(m_movies.size());
}

int MovieTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant MovieTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_movies.size())
        return {};

    const auto& m = m_movies[index.row()];
    const auto col = static_cast<Column>(index.column());

    // ---- BoxSetParentIdRole is column-independent ----------------------------
    if (role == BoxSetParentIdRole)
        return m.boxSet.parentId;

    // ---- Qt::DisplayRole — formatted strings for the view -------------------
    if (role == Qt::DisplayRole) {
        switch (col) {
        case Title:         return m.title;
        case OriginalTitle: return m.originalTitle;
        case SortTitle:     return m.sortTitle.isEmpty() ? m.title : m.sortTitle;
        case Year:          return m.productionYear != 0
                                   ? QString::number(m.productionYear)
                                   : QString();
        case Runtime:       return m.runningTimeMinutes != 0
                                   ? QStringLiteral("%1 min").arg(m.runningTimeMinutes)
                                   : QString();
        case Format:        return m.format;
        case Rating:        return m.rating.value;
        case RatingAge:     return m.rating.age != 0
                                   ? QString::number(m.rating.age)
                                   : QString();
        case Director:      return primaryDirector(m);
        case Genres:        return m.genres.join(QStringLiteral(", "));
        case Studios:       return m.studios.join(QStringLiteral(", "));
        case CaseType:      return m.caseType;
        case AspectRatio:   return m.videoFormat.aspectRatio;
        case TmdbId:        return m.tmdbId != 0
                                   ? QString::number(m.tmdbId)
                                   : QString();
        case Loaned:        return m.loan.loaned
                                   ? QStringLiteral("✓")
                                   : QString();
        case BoxSetParent:  return m.boxSet.isParent
                                   ? QStringLiteral("✓")
                                   : QString();
        case ColumnCount:   break;
        }
        return {};
    }

    // ---- Qt::UserRole — raw values for sorting / filtering ------------------
    if (role == Qt::UserRole) {
        switch (col) {
        case Title:         return m.title;
        case OriginalTitle: return m.originalTitle;
        case SortTitle:     return m.sortTitle.isEmpty() ? m.title : m.sortTitle;
        case Year:          return m.productionYear;
        case Runtime:       return m.runningTimeMinutes;
        case Format:        return m.format;
        case Rating:        return m.rating.value;
        case RatingAge:     return m.rating.age;
        case Director:      return primaryDirector(m);
        case Genres:        return m.genres.join(QStringLiteral(", "));
        case Studios:       return m.studios.join(QStringLiteral(", "));
        case CaseType:      return m.caseType;
        case AspectRatio:   return m.videoFormat.aspectRatio;
        case TmdbId:        return m.tmdbId;
        case Loaned:        return m.loan.loaned;
        case BoxSetParent:  return m.boxSet.isParent;
        case ColumnCount:   break;
        }
        return {};
    }

    // ---- Qt::FontRole — bold for box-set parents ----------------------------
    if (role == Qt::FontRole) {
        if (m.boxSet.isParent) {
            QFont font;
            font.setBold(true);
            return font;
        }
        return {};
    }

    // ---- Qt::TextAlignmentRole — right-align numeric columns ----------------
    if (role == Qt::TextAlignmentRole) {
        switch (col) {
        case Year:
        case Runtime:
        case RatingAge:
        case TmdbId:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return {};
        }
    }

    return {};
}

QVariant MovieTableModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (static_cast<Column>(section)) {
    case Title:         return tr("Title");
    case OriginalTitle: return tr("Original Title");
    case SortTitle:     return tr("Sort Title");
    case Year:          return tr("Year");
    case Runtime:       return tr("Runtime");
    case Format:        return tr("Format");
    case Rating:        return tr("Rating");
    case RatingAge:     return tr("Age");
    case Director:      return tr("Director");
    case Genres:        return tr("Genres");
    case Studios:       return tr("Studios");
    case CaseType:      return tr("Case");
    case AspectRatio:   return tr("Aspect Ratio");
    case TmdbId:        return tr("TMDb ID");
    case Loaned:        return tr("Loaned");
    case BoxSetParent:  return tr("Box Set");
    case ColumnCount:   break;
    }
    return {};
}

void MovieTableModel::setMovies(QList<Movie> movies)
{
    beginResetModel();
    m_movies = std::move(movies);
    endResetModel();
}

const Movie* MovieTableModel::find(const QString& id) const
{
    for (const auto& m : m_movies) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

int MovieTableModel::indexOfId(const QString& id) const
{
    for (int i = 0; i < m_movies.size(); ++i) {
        if (m_movies[i].id == id) return i;
    }
    return -1;
}

QString MovieTableModel::movieIdAtRow(int row) const
{
    if (row < 0 || row >= m_movies.size())
        return {};
    return m_movies[row].id;
}

} // namespace xyz
