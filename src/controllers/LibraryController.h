#pragma once

#include "domain/Movie.h"
#include "models/MovieListModel.h"
#include "models/MovieSortProxyModel.h"
#include "models/MovieTableModel.h"
#include "models/MovieTreeModel.h"
#include "tmdb/TmdbTypes.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace xyz {

class Database;
class MovieRepository;
class TmdbClient;

class LibraryController : public QObject {
    Q_OBJECT

public:
    explicit LibraryController(QObject* parent = nullptr);
    ~LibraryController() override;

    void setTmdbClient(TmdbClient* client);

    // ---- Library state ------------------------------------------------------
    QString libraryPath()   const { return m_libraryPath; }
    bool    libraryOpen()   const;
    int     movieCount()    const;
    QString statusMessage() const { return m_statusMessage; }

    // ---- Models for views ---------------------------------------------------
    MovieListModel*      listModel()  const { return m_listModel.get(); }
    MovieTableModel*     tableModel() const { return m_tableModel.get(); }
    MovieTreeModel*      treeModel()  const { return m_treeModel.get(); }
    MovieSortProxyModel* sortProxy()  const { return m_sortProxy.get(); }

    // ---- Selection ----------------------------------------------------------
    bool    hasSelection()    const { return !m_selectedId.isEmpty(); }
    QString selectedId()      const { return m_selectedId; }
    const Movie& selectedMovie() const { return m_selected; }

    // ---- Import state -------------------------------------------------------
    bool    importInProgress() const { return m_importInProgress; }
    QString importStage()      const { return m_importStage; }
    int     importCurrent()    const { return m_importCurrent; }
    int     importTotal()      const { return m_importTotal; }

    bool        previewActive()       const { return m_previewActive; }
    int         previewMovieCount()   const { return int(m_previewMovies.size()); }
    QStringList previewSampleTitles() const;
    QString     previewSourceName()   const { return m_previewSourceName; }
    QString     previewImagesDir()    const { return m_previewImagesDir; }

    // ---- TMDb state ---------------------------------------------------------
    bool    tmdbAvailable()   const { return m_tmdb != nullptr; }
    bool    tmdbSearching()   const { return m_tmdbSearching; }
    QString tmdbSearchError() const { return m_tmdbSearchError; }
    const QList<TmdbCandidate>& tmdbCandidates() const { return m_tmdbCandidates; }

    // ---- Actions ------------------------------------------------------------
    bool openLibrary(const QString& path);

    bool beginImport(const QString& xmlPath, const QString& imagesDir = {});
    bool commitImport();
    void cancelImport();
    bool importDvdProfilerXml(const QString& xmlPath, const QString& imagesDir = {});

    void refresh();
    void search(const QString& query);
    void selectMovie(const QString& id);
    void clearSelection();

    void searchSelectedOnTmdb();
    void pickTmdbMatch(int tmdbId);
    void clearTmdbCandidates();

signals:
    void libraryChanged();
    void moviesChanged();
    void statusMessageChanged();
    void selectionChanged();
    void importStateChanged();
    void importFinished(int imported, const QString& errorString);
    void tmdbStateChanged();
    void tmdbMatchPicked(const QString& movieId, int tmdbId);
    // A cover file on disk was replaced in place (same path, new bytes). The UI
    // uses this to invalidate its path-keyed pixmap caches before repainting.
    void coverUpdated(const QString& path);

private:
    void setStatus_(const QString& message);

    struct ParseOutcome {
        QList<Movie> movies;
        QString      errorString;
    };
    struct ImportOutcome {
        int     imported = 0;
        QString errorString;
    };
    void onImportProgressChanged_(int current);
    void onImportRangeChanged_(int min, int max);
    void onImportFinished_();
    void setImportStage_(const QString& stage);
    void onParseFinished_();
    void runWriter_(QList<Movie> movies, bool autoCommit);
    void downloadTmdbPoster_(const QString& movieId, const QString& posterPath);
    void setMoviesOnBothModels_(QList<Movie> movies);
    static QList<Movie> groupByBoxSet_(QList<Movie> movies);

    std::unique_ptr<Database>            m_db;
    std::unique_ptr<MovieRepository>     m_repo;
    std::unique_ptr<MovieListModel>      m_listModel;
    std::unique_ptr<MovieTableModel>     m_tableModel;
    std::unique_ptr<MovieTreeModel>      m_treeModel;
    std::unique_ptr<MovieSortProxyModel> m_sortProxy;

    QString m_libraryPath;
    QString m_libraryRoot;
    QString m_statusMessage;

    Movie   m_selected;
    QString m_selectedId;

    bool                            m_importInProgress = false;
    QString                         m_importStage;
    int                             m_importCurrent = 0;
    int                             m_importTotal   = 0;
    QFutureWatcher<ImportOutcome>   m_importWatcher;
    QFutureWatcher<ParseOutcome>    m_parseWatcher;

    bool                            m_previewActive = false;
    QList<Movie>                    m_previewMovies;
    QString                         m_previewSourceName;
    QString                         m_previewImagesDir;

    TmdbClient*                     m_tmdb = nullptr;
    bool                            m_tmdbSearching = false;
    QString                         m_tmdbSearchError;
    QString                         m_tmdbSearchingForId;
    QList<TmdbCandidate>            m_tmdbCandidates;
};

} // namespace xyz
