#include "TmdbClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace xyz {
namespace {

constexpr auto kApiBase = "https://api.themoviedb.org/3";

// TMDb's `language` parameter expects an ISO 639-1 code, optionally combined
// with an ISO 3166-1 region (e.g. "de-DE"). QLocale::name() yields the
// underscore form ("de_AT"), so swap the separator. When a region-specific
// translation is missing TMDb falls back to the base language, so "de-AT"
// still returns German metadata. Mirrors how main.cpp picks the UI language
// from QLocale(), keeping search results and the interface in the same
// language.
QString defaultTmdbLanguage()
{
    QString name = QLocale().name(); // e.g. "de_AT", "en_US"
    name.replace(QLatin1Char('_'), QLatin1Char('-'));
    return name.isEmpty() ? QStringLiteral("en-US") : name;
}

QString jsonStr(const QJsonObject& o, const char* key)
{
    const auto v = o.value(QLatin1String(key));
    return v.isString() ? v.toString() : QString();
}

int jsonInt(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toInt(0);
}

double jsonDouble(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toDouble(0.0);
}

QStringList jsonStringList(const QJsonObject& o, const char* key, const char* itemKey)
{
    QStringList out;
    const auto arr = o.value(QLatin1String(key)).toArray();
    for (const auto& v : arr) {
        const auto item = v.toObject().value(QLatin1String(itemKey));
        if (item.isString()) out << item.toString();
    }
    return out;
}

TmdbCandidate parseCandidate(const QJsonObject& o)
{
    TmdbCandidate c;
    c.id               = jsonInt   (o, "id");
    c.title            = jsonStr   (o, "title");
    c.originalTitle    = jsonStr   (o, "original_title");
    c.originalLanguage = jsonStr   (o, "original_language");
    c.releaseDate      = jsonStr   (o, "release_date");
    c.overview         = jsonStr   (o, "overview");
    c.posterPath       = jsonStr   (o, "poster_path");
    c.popularity       = jsonDouble(o, "popularity");
    c.voteAverage      = jsonDouble(o, "vote_average");
    c.voteCount        = jsonInt   (o, "vote_count");
    for (const auto& g : o.value(QLatin1String("genre_ids")).toArray())
        c.genreIds << g.toInt();
    return c;
}

void sortCandidates(QList<TmdbCandidate>& v, const QString& sortBy)
{
    if (sortBy.startsWith(QLatin1String("vote_average")))
        std::sort(v.begin(), v.end(), [](const TmdbCandidate& a, const TmdbCandidate& b) {
            return a.voteAverage > b.voteAverage; });
    else if (sortBy.startsWith(QLatin1String("primary_release_date.asc"))
          || sortBy.startsWith(QLatin1String("release_date.asc")))
        std::sort(v.begin(), v.end(), [](const TmdbCandidate& a, const TmdbCandidate& b) {
            return a.releaseDate < b.releaseDate; });
    else if (sortBy.startsWith(QLatin1String("primary_release_date.desc"))
          || sortBy.startsWith(QLatin1String("release_date.desc")))
        std::sort(v.begin(), v.end(), [](const TmdbCandidate& a, const TmdbCandidate& b) {
            return a.releaseDate > b.releaseDate; });
    else if (sortBy.startsWith(QLatin1String("title"))
          || sortBy.startsWith(QLatin1String("original_title")))
        std::sort(v.begin(), v.end(), [](const TmdbCandidate& a, const TmdbCandidate& b) {
            return a.title.compare(b.title, Qt::CaseInsensitive) < 0; });
    else
        std::sort(v.begin(), v.end(), [](const TmdbCandidate& a, const TmdbCandidate& b) {
            return a.popularity > b.popularity; });
}

// /search/movie returns title-matched hits but ignores structured filters, so
// apply the ones we can (year, rating, language, genres) on the client side.
QList<TmdbCandidate> applyClientFilters(QList<TmdbCandidate> hits,
                                        const TmdbDiscoverQuery& dq)
{
    QList<TmdbCandidate> out;
    out.reserve(hits.size());
    for (const auto& c : hits) {
        const int y = c.year();
        if (dq.yearMin > 0 && (y == 0 || y < dq.yearMin)) continue;
        if (dq.yearMax > 0 && (y == 0 || y > dq.yearMax)) continue;
        if (dq.voteAverageMin > 0.0 && c.voteAverage < dq.voteAverageMin) continue;
        if (!dq.originalLanguage.isEmpty() && c.originalLanguage != dq.originalLanguage)
            continue;
        if (!dq.genreIds.isEmpty()) {
            bool hasAll = true;
            for (int g : dq.genreIds)
                if (!c.genreIds.contains(g)) { hasAll = false; break; }
            if (!hasAll) continue;
        }
        out << c;
    }
    sortCandidates(out, dq.sortBy);
    return out;
}

TmdbMovieDetails parseDetails(const QJsonObject& o)
{
    TmdbMovieDetails d;
    d.id                  = jsonInt   (o, "id");
    d.title               = jsonStr   (o, "title");
    d.originalTitle       = jsonStr   (o, "original_title");
    d.originalLanguage    = jsonStr   (o, "original_language");
    d.releaseDate         = jsonStr   (o, "release_date");
    d.overview            = jsonStr   (o, "overview");
    d.tagline             = jsonStr   (o, "tagline");
    d.homepage            = jsonStr   (o, "homepage");
    d.status              = jsonStr   (o, "status");
    d.posterPath          = jsonStr   (o, "poster_path");
    d.backdropPath        = jsonStr   (o, "backdrop_path");
    d.runtime             = jsonInt   (o, "runtime");
    d.budget              = jsonInt   (o, "budget");
    d.revenue             = jsonInt   (o, "revenue");
    d.adult               = o.value(QLatin1String("adult")).toBool();
    d.popularity          = jsonDouble(o, "popularity");
    d.voteAverage         = jsonDouble(o, "vote_average");
    d.voteCount           = jsonInt   (o, "vote_count");
    d.imdbId              = jsonStr   (o, "imdb_id");
    d.genres              = jsonStringList(o, "genres",              "name");
    d.productionCompanies = jsonStringList(o, "production_companies","name");
    d.productionCountries = jsonStringList(o, "production_countries","name");
    d.spokenLanguages     = jsonStringList(o, "spoken_languages",    "english_name");
    return d;
}

TmdbImageConfig parseImageConfig(const QJsonObject& root)
{
    TmdbImageConfig c;
    const auto images = root.value(QLatin1String("images")).toObject();
    c.secureBaseUrl = jsonStr(images, "secure_base_url");
    for (const auto& v : images.value(QLatin1String("poster_sizes")).toArray()) {
        c.posterSizes << v.toString();
    }
    for (const auto& v : images.value(QLatin1String("backdrop_sizes")).toArray()) {
        c.backdropSizes << v.toString();
    }
    return c;
}

QString replyErrorString(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) return {};
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString msg = reply->errorString();
    // TMDb returns a JSON error body with status_message — surface that
    // when present, it's more useful than Qt's generic "Server returned 4xx".
    const auto body = reply->readAll();
    const auto doc  = QJsonDocument::fromJson(body);
    if (doc.isObject()) {
        const auto sm = doc.object().value(QLatin1String("status_message")).toString();
        if (!sm.isEmpty()) msg = sm;
    }
    if (httpStatus > 0) {
        return QStringLiteral("HTTP %1: %2").arg(httpStatus).arg(msg);
    }
    return msg;
}

} // namespace

int TmdbCandidate::year() const
{
    if (releaseDate.size() < 4) return 0;
    return releaseDate.left(4).toInt();
}

TmdbClient::TmdbClient(QString apiKey, QNetworkAccessManager* network, QObject* parent)
    : QObject(parent),
      m_apiKey(std::move(apiKey)),
      m_network(network),
      m_ownsNetwork(network == nullptr),
      m_language(defaultTmdbLanguage())
{
    if (m_ownsNetwork) m_network = new QNetworkAccessManager(this);
}

TmdbClient::~TmdbClient() = default;

QString TmdbClient::imageUrl(const QString& filePath, const QString& size) const
{
    if (filePath.isEmpty()) return {};
    QString base = m_imageConfig.secureBaseUrl;
    if (base.isEmpty()) {
        // Fallback when /configuration hasn't been fetched yet. TMDb's
        // base URL has been stable for years, so this is safe enough for
        // the v1 flow.
        base = QStringLiteral("https://image.tmdb.org/t/p/");
    }
    return base + size + filePath;
}

QNetworkReply* TmdbClient::get_(const QString& path, const QUrlQuery& extra) const
{
    QUrl url(QString::fromLatin1(kApiBase) + path);
    QUrlQuery query = extra;
    query.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::PreferCache);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("xyz-profiler/0.3 (+https://github.com)"));
    return m_network->get(req);
}

void TmdbClient::emitSearchError_(const QString& title, int year, const QString& err)
{
    emit searchFinished(title, year, {}, err);
}

void TmdbClient::search(const QString& title, int year)
{
    if (!hasApiKey()) {
        emitSearchError_(title, year, tr("TMDB_API_KEY is not configured"));
        return;
    }
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), title);
    q.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    q.addQueryItem(QStringLiteral("language"), m_language);
    if (year > 0) {
        q.addQueryItem(QStringLiteral("year"), QString::number(year));
    }
    auto* reply = get_(QStringLiteral("/search/movie"), q);
    connect(reply, &QNetworkReply::finished, this, [this, reply, title, year]() {
        reply->deleteLater();
        const QString err = replyErrorString(reply);
        if (!err.isEmpty()) {
            emitSearchError_(title, year, err);
            return;
        }
        const auto doc  = QJsonDocument::fromJson(reply->readAll());
        const auto root = doc.object();
        QList<TmdbCandidate> hits;
        for (const auto& v : root.value(QLatin1String("results")).toArray()) {
            hits << parseCandidate(v.toObject());
        }
        emit searchFinished(title, year, hits, {});
    });
}

void TmdbClient::getMovie(int tmdbId)
{
    if (!hasApiKey()) {
        emit movieFinished({}, tr("TMDB_API_KEY is not configured"));
        return;
    }
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("language"), m_language);
    auto* reply = get_(QStringLiteral("/movie/") + QString::number(tmdbId), q);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QString err = replyErrorString(reply);
        if (!err.isEmpty()) {
            emit movieFinished({}, err);
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit movieFinished(parseDetails(doc.object()), {});
    });
}

void TmdbClient::discover(const TmdbDiscoverQuery& dq)
{
    if (!hasApiKey()) {
        emit discoverFinished({}, tr("TMDB_API_KEY is not configured"));
        return;
    }

    const QString title = dq.title.trimmed();
    if (!title.isEmpty()) {
        // Free-text title → /search/movie, then filter the rest client-side.
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("query"), title);
        q.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
        q.addQueryItem(QStringLiteral("language"), m_language);
        if (dq.yearMin > 0 && dq.yearMin == dq.yearMax)
            q.addQueryItem(QStringLiteral("year"), QString::number(dq.yearMin));
        auto* reply = get_(QStringLiteral("/search/movie"), q);
        connect(reply, &QNetworkReply::finished, this, [this, reply, dq]() {
            reply->deleteLater();
            const QString err = replyErrorString(reply);
            if (!err.isEmpty()) { emit discoverFinished({}, err); return; }
            const auto root = QJsonDocument::fromJson(reply->readAll()).object();
            QList<TmdbCandidate> hits;
            for (const auto& v : root.value(QLatin1String("results")).toArray())
                hits << parseCandidate(v.toObject());
            emit discoverFinished(applyClientFilters(hits, dq), {});
        });
        return;
    }

    const QString person = dq.person.trimmed();
    if (!person.isEmpty()) {
        // Resolve the person to an id first, then discover with_people.
        QUrlQuery pq;
        pq.addQueryItem(QStringLiteral("query"), person);
        pq.addQueryItem(QStringLiteral("language"), m_language);
        auto* reply = get_(QStringLiteral("/search/person"), pq);
        connect(reply, &QNetworkReply::finished, this, [this, reply, dq]() {
            reply->deleteLater();
            const QString err = replyErrorString(reply);
            if (!err.isEmpty()) { emit discoverFinished({}, err); return; }
            const auto root = QJsonDocument::fromJson(reply->readAll()).object();
            const auto results = root.value(QLatin1String("results")).toArray();
            const int personId = results.isEmpty()
                ? 0
                : results.first().toObject().value(QLatin1String("id")).toInt();
            if (personId == 0) {
                emit discoverFinished({},
                    tr("No person found matching \"%1\".").arg(dq.person));
                return;
            }
            runDiscover_(dq, personId);
        });
        return;
    }

    runDiscover_(dq, 0);
}

void TmdbClient::runDiscover_(const TmdbDiscoverQuery& dq, int personId)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("language"), m_language);
    q.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
    q.addQueryItem(QStringLiteral("sort_by"),
                   dq.sortBy.isEmpty() ? QStringLiteral("popularity.desc") : dq.sortBy);
    if (!dq.genreIds.isEmpty()) {
        QStringList ids;
        for (int g : dq.genreIds) ids << QString::number(g);
        q.addQueryItem(QStringLiteral("with_genres"), ids.join(QChar(u',')));
    }
    if (dq.yearMin > 0)
        q.addQueryItem(QStringLiteral("primary_release_date.gte"),
                       QStringLiteral("%1-01-01").arg(dq.yearMin));
    if (dq.yearMax > 0)
        q.addQueryItem(QStringLiteral("primary_release_date.lte"),
                       QStringLiteral("%1-12-31").arg(dq.yearMax));
    if (dq.voteAverageMin > 0.0) {
        q.addQueryItem(QStringLiteral("vote_average.gte"),
                       QString::number(dq.voteAverageMin, 'f', 1));
        // Keep obscure single-vote outliers out of rating-filtered results.
        q.addQueryItem(QStringLiteral("vote_count.gte"), QStringLiteral("50"));
    }
    if (!dq.originalLanguage.isEmpty())
        q.addQueryItem(QStringLiteral("with_original_language"), dq.originalLanguage);
    if (!dq.originCountry.isEmpty())
        q.addQueryItem(QStringLiteral("with_origin_country"), dq.originCountry);
    if (dq.runtimeMin > 0)
        q.addQueryItem(QStringLiteral("with_runtime.gte"), QString::number(dq.runtimeMin));
    if (dq.runtimeMax > 0)
        q.addQueryItem(QStringLiteral("with_runtime.lte"), QString::number(dq.runtimeMax));
    if (personId > 0)
        q.addQueryItem(QStringLiteral("with_people"), QString::number(personId));

    auto* reply = get_(QStringLiteral("/discover/movie"), q);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QString err = replyErrorString(reply);
        if (!err.isEmpty()) { emit discoverFinished({}, err); return; }
        const auto root = QJsonDocument::fromJson(reply->readAll()).object();
        QList<TmdbCandidate> hits;
        for (const auto& v : root.value(QLatin1String("results")).toArray())
            hits << parseCandidate(v.toObject());
        emit discoverFinished(hits, {});
    });
}

const QList<TmdbGenre>& TmdbClient::movieGenres()
{
    static const QList<TmdbGenre> genres = {
        {28,    QStringLiteral("Action")},
        {12,    QStringLiteral("Adventure")},
        {16,    QStringLiteral("Animation")},
        {35,    QStringLiteral("Comedy")},
        {80,    QStringLiteral("Crime")},
        {99,    QStringLiteral("Documentary")},
        {18,    QStringLiteral("Drama")},
        {10751, QStringLiteral("Family")},
        {14,    QStringLiteral("Fantasy")},
        {36,    QStringLiteral("History")},
        {27,    QStringLiteral("Horror")},
        {10402, QStringLiteral("Music")},
        {9648,  QStringLiteral("Mystery")},
        {10749, QStringLiteral("Romance")},
        {878,   QStringLiteral("Science Fiction")},
        {10770, QStringLiteral("TV Movie")},
        {53,    QStringLiteral("Thriller")},
        {10752, QStringLiteral("War")},
        {37,    QStringLiteral("Western")},
    };
    return genres;
}

void TmdbClient::fetchConfiguration()
{
    if (!hasApiKey()) {
        emit configurationFetched({}, tr("TMDB_API_KEY is not configured"));
        return;
    }
    auto* reply = get_(QStringLiteral("/configuration"), {});
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QString err = replyErrorString(reply);
        if (!err.isEmpty()) {
            emit configurationFetched({}, err);
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        m_imageConfig = parseImageConfig(doc.object());
        emit configurationFetched(m_imageConfig, {});
    });
}

} // namespace xyz
