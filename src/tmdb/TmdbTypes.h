#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace xyz {

// One hit from /search/movie or /discover/movie. Just the columns we need for
// the candidate lists — the full detail object is fetched separately via
// /movie/{id} once the user picks a candidate.
struct TmdbCandidate {
    int     id = 0;                    // TMDb's numeric movie id
    QString title;
    QString originalTitle;
    QString originalLanguage;          // ISO 639-1 code
    QString releaseDate;               // YYYY-MM-DD
    int     year() const;              // parsed from releaseDate
    QString overview;
    QString posterPath;                // "/abc.jpg" — combine with image base URL
    double  popularity = 0.0;
    double  voteAverage = 0.0;
    int     voteCount = 0;
    QList<int> genreIds;               // TMDb genre ids (from search/discover)
};

// A movie genre — TMDb's stable numeric id paired with its name.
struct TmdbGenre {
    int     id = 0;
    QString name;
};

// One confirmed bulk match: which collection movie links to which TMDb id, plus
// the poster path to fetch. Produced by BulkTmdbMatchDialog, consumed by
// LibraryController::applyTmdbMatches.
struct TmdbBulkMatch {
    QString movieId;
    int     tmdbId = 0;
    QString posterPath;
};

// Rich "add a title" search/filter criteria. When `title` is non-empty the
// client uses /search/movie (free-text match) and applies the remaining
// filters client-side; when it is empty it uses /discover/movie with every
// filter applied server-side. `person` is resolved to an id via /search/person.
struct TmdbDiscoverQuery {
    QString    title;
    int        yearMin = 0;
    int        yearMax = 0;
    QList<int> genreIds;
    QString    person;
    double     voteAverageMin = 0.0;
    QString    originalLanguage;       // ISO 639-1, empty = any
    QString    originCountry;          // ISO 3166-1, empty = any
    int        runtimeMin = 0;         // minutes
    int        runtimeMax = 0;
    QString    sortBy = QStringLiteral("popularity.desc");
};

// Detail response from /movie/{id}. Superset of TmdbCandidate fields plus
// the per-movie metadata we'll eventually want to merge into MediaItem.
struct TmdbMovieDetails {
    int     id = 0;
    QString title;
    QString originalTitle;
    QString originalLanguage;
    QString releaseDate;
    QString overview;
    QString tagline;
    QString homepage;
    QString status;
    QString posterPath;
    QString backdropPath;
    int     runtime = 0;               // minutes
    int     budget = 0;
    int     revenue = 0;
    bool    adult = false;
    double  popularity = 0.0;
    double  voteAverage = 0.0;
    int     voteCount = 0;
    QStringList genres;
    QStringList productionCompanies;
    QStringList productionCountries;
    QStringList spokenLanguages;
    QString imdbId;
};

// Image configuration from /configuration. Cached after first fetch; used
// to compose full image URLs as `{base}{size}{posterPath}`.
struct TmdbImageConfig {
    QString     secureBaseUrl;         // e.g. "https://image.tmdb.org/t/p/"
    QStringList posterSizes;           // ["w92","w154","w185","w342","w500","w780","original"]
    QStringList backdropSizes;
};

} // namespace xyz
