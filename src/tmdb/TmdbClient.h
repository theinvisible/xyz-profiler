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

    // Cached image configuration — empty until the first successful
    // fetchConfiguration() call. Compose URLs with
    // `imageUrl(posterPath, size)`.
    TmdbImageConfig configuration() const { return m_imageConfig; }
    QString imageUrl(const QString& filePath,
                     const QString& size = QStringLiteral("w185")) const;

    // ---- API actions -----------------------------------------------------
    void search(const QString& title, int year = 0);
    void getMovie(int tmdbId);
    void fetchConfiguration();

signals:
    void searchFinished(const QString& title, int year,
                        const QList<TmdbCandidate>& candidates,
                        const QString& errorString);
    void movieFinished(const TmdbMovieDetails& details,
                       const QString& errorString);
    void configurationFetched(const TmdbImageConfig& config,
                              const QString& errorString);

private:
    QNetworkReply* get_(const QString& path, const QUrlQuery& extra) const;
    void emitSearchError_(const QString& title, int year, const QString& err);

    QString                 m_apiKey;
    QNetworkAccessManager*  m_network;
    bool                    m_ownsNetwork;
    TmdbImageConfig         m_imageConfig;
};

} // namespace xyz
