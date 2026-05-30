#include "TmdbClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

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
    return c;
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
