#include "MovieListModel.h"

#include <QUrl>
#include <QVariant>

namespace xyz {
namespace {

// Find the first credit with `creditType == "Direction"`. Falls back to
// the first credit overall; returns an empty string if none.
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

// QML Image expects either an absolute file:// URL or a qrc:/ path.
// Plain local paths are still accepted by QML but get noisy warnings;
// converting once here keeps delegates simple.
QString coverUrl(const QString& path)
{
    if (path.isEmpty()) return {};
    return QUrl::fromLocalFile(path).toString();
}

} // namespace

MovieListModel::MovieListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int MovieListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return int(m_movies.size());
}

QVariant MovieListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_movies.size())
        return {};
    const auto& m = m_movies[index.row()];
    switch (role) {
    case IdRole:               return m.id;
    case TitleRole:            return m.title;
    case OriginalTitleRole:    return m.originalTitle;
    case SortTitleRole:        return m.sortTitle.isEmpty() ? m.title : m.sortTitle;
    case YearRole:             return m.productionYear;
    case RuntimeRole:          return m.runningTimeMinutes;
    case FormatRole:           return m.format;
    case CoverFrontPathRole:   return coverUrl(m.coverFrontPath);
    case CoverBackPathRole:    return coverUrl(m.coverBackPath);
    case DirectorNameRole:     return primaryDirector(m);
    case RatingValueRole:      return m.rating.value;
    case RatingAgeRole:        return m.rating.age;
    case GenresJoinedRole:     return m.genres.join(QStringLiteral(", "));
    case IsLoanedRole:         return m.loan.loaned;
    case IsBoxSetParentRole:   return m.boxSet.isParent;
    default:                   return {};
    }
}

QHash<int, QByteArray> MovieListModel::roleNames() const
{
    return {
        {IdRole,             "id"},
        {TitleRole,          "title"},
        {OriginalTitleRole,  "originalTitle"},
        {SortTitleRole,      "sortTitle"},
        {YearRole,           "year"},
        {RuntimeRole,        "runtime"},
        {FormatRole,         "format"},
        {CoverFrontPathRole, "coverFrontPath"},
        {CoverBackPathRole,  "coverBackPath"},
        {DirectorNameRole,   "directorName"},
        {RatingValueRole,    "ratingValue"},
        {RatingAgeRole,      "ratingAge"},
        {GenresJoinedRole,   "genresJoined"},
        {IsLoanedRole,       "isLoaned"},
        {IsBoxSetParentRole, "isBoxSetParent"},
    };
}

void MovieListModel::setMovies(QList<Movie> movies)
{
    beginResetModel();
    m_movies = std::move(movies);
    endResetModel();
}

const Movie* MovieListModel::find(const QString& id) const
{
    for (const auto& m : m_movies) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

int MovieListModel::indexOfId(const QString& id) const
{
    for (int i = 0; i < m_movies.size(); ++i) {
        if (m_movies[i].id == id) return i;
    }
    return -1;
}

} // namespace xyz
