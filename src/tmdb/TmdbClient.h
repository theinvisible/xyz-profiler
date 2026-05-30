#pragma once

#include "tmdb/TmdbTypes.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QUrlQuery;

namespace xyz {

// Thin async wrapper around the TMDb v3 read API.
//
// The client holds a single QNetworkAccessManager (caller-supplied if a
// shared cache is desired — see Application setup; falls back to an
// internal one otherwise). All endpoints emit a signal on completion with
// either the parsed result or a non-empty errorString.
//
// API-key handling: read-only v3 endpoints take `?api_key=<key>`. If the
// key is empty the client emits a synthetic error rather than firing a
// request, so callers can degrade gracefully in environments where the
// key isn't configured.
class TmdbClient : public QObject {
    Q_OBJECT

public:
    explicit TmdbClient(QString apiKey,
                        QNetworkAccessManager* network = nullptr,
                        QObject* parent = nullptr);
    ~TmdbClient() override;

    void    setApiKey(const QString& key) { m_apiKey = key; }
    QString apiKey() const { return m_apiKey; }
    bool    hasApiKey() const { return !m_apiKey.isEmpty(); }

    // TMDb `language` parameter (ISO 639-1, optionally with region — e.g.
    // "de-DE"). Defaults to the application locale so search results and
    // movie details match the UI language; override for tests or an explicit
    // user preference.
    void    setLanguage(const QString& language) { m_language = language; }
    QString language() const { return m_language; }

    QNetworkAccessManager* network() const { return m_network; }

    // Cached image configuration — empty until the first successful
    // fetchConfiguration() call. Compose URLs with
    // `imageUrl(posterPath, size)`.
    TmdbImageConfig configuration() const { return m_imageConfig; }
    QString imageUrl(const QString& filePath,
                     const QString& size = QStringLiteral("w185")) const;

    // ---- API actions -----------------------------------------------------
    void search(const QString& title, int year = 0);
    // Like search(), but the result is reported via searchForFinished() tagged
    // with the caller's requestId. Lets a bulk caller correlate each reply to a
    // specific movie even when titles/years collide (search()'s title/year are
    // ambiguous across a large collection).
    void searchFor(quint64 requestId, const QString& title, int year = 0);
    void getMovie(int tmdbId);
    void fetchConfiguration();

    // Rich "add a title" search. See TmdbDiscoverQuery: routes to /search/movie
    // (with client-side filtering) when a title is given, otherwise to
    // /discover/movie with every filter applied server-side.
    void discover(const TmdbDiscoverQuery& query);

    // Standard TMDb movie genres (stable ids + names). Lets the add-title
    // filter offer a genre picker without an extra network round-trip.
    static const QList<TmdbGenre>& movieGenres();

signals:
    void searchFinished(const QString& title, int year,
                        const QList<TmdbCandidate>& candidates,
                        const QString& errorString);
    void searchForFinished(quint64 requestId,
                           const QList<TmdbCandidate>& candidates,
                           const QString& errorString);
    void movieFinished(const TmdbMovieDetails& details,
                       const QString& errorString);
    void configurationFetched(const TmdbImageConfig& config,
                              const QString& errorString);
    void discoverFinished(const QList<TmdbCandidate>& candidates,
                          const QString& errorString);

private:
    QNetworkReply* get_(const QString& path, const QUrlQuery& extra) const;
    void emitSearchError_(const QString& title, int year, const QString& err);
    void runDiscover_(const TmdbDiscoverQuery& query, int personId);

    QString                 m_apiKey;
    QNetworkAccessManager*  m_network;
    bool                    m_ownsNetwork;
    QString                 m_language;
    TmdbImageConfig         m_imageConfig;
};

} // namespace xyz
