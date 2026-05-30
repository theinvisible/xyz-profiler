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
#include <functional>
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

    // Persist user edits to an existing entry (the edit dialog mutates a copy of
    // the full Movie, so unedited fields — box set, tmdbId, cast — are kept).
    void updateMovie(const Movie& edited);
    // Delete an entry by id (after the UI has confirmed). Refreshes the views.
    void deleteMovie(const QString& id);
    // Create a brand-new collection entry from a TMDb movie (the "add title"
    // flow): fetches full details, inserts, selects it, and downloads the poster.
    // `format` is the disc format the user chose ("DVD" / "BluRay" / "UHD").
    void addMovieFromTmdb(int tmdbId, const QString& format,
                          const QString& posterPath = {});
    void downloadTmdbPosterForMovie(const QString& movieId, const QString& posterPath);

    // Bulk-link many movies to TMDb at once. Writes only the tmdbId of each
    // match on a worker thread (one transaction), then — if downloadPosters —
    // fetches their posters through a throttled queue. Imported metadata is
    // left untouched. See TmdbBulkMatch.
    void applyTmdbMatches(const QList<TmdbBulkMatch>& matches, bool downloadPosters);

    // Bulk-match progress (for a QProgressDialog), valid while
    // bulkMatchInProgress() is true.
    bool    bulkMatchInProgress() const { return m_bulkInProgress; }
    QString bulkMatchStage()      const { return m_bulkStage; }
    int     bulkMatchCurrent()    const { return m_bulkCurrent; }
    int     bulkMatchTotal()      const { return m_bulkTotal; }

signals:
    void libraryChanged();
    void moviesChanged();
    void statusMessageChanged();
    void selectionChanged();
    void importStateChanged();
    void importFinished(int imported, const QString& errorString);
    void tmdbStateChanged();
    void tmdbMatchPicked(const QString& movieId, int tmdbId);
    // Bulk-match lifecycle: state changes drive the progress dialog; finished
    // reports how many links were written.
    void bulkMatchStateChanged();
    void bulkMatchFinished(int matched, const QString& errorString);
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
    void downloadTmdbPoster_(const QString& movieId, const QString& posterPath,
                             const std::function<void()>& onDone = {});
    void setMoviesOnBothModels_(QList<Movie> movies);
    static QList<Movie> groupByBoxSet_(QList<Movie> movies);

    void onBulkWriteFinished_();
    void pumpPosterQueue_();   // start downloads up to the in-flight cap

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

    // "Add title" flow: set while a getMovie() fetch for a new entry is in
    // flight, so the shared movieFinished handler knows to create a Movie.
    int                             m_addingTmdbId = 0;
    QString                         m_addingPosterPath;
    QString                         m_addingFormat;

    // Bulk TMDb match: worker-thread DB write + throttled poster downloads.
    bool                            m_bulkInProgress = false;
    QString                         m_bulkStage;
    int                             m_bulkCurrent = 0;
    int                             m_bulkTotal   = 0;
    QFutureWatcher<ImportOutcome>   m_bulkWriteWatcher;   // reuses ImportOutcome
    QList<TmdbBulkMatch>            m_posterQueue;        // pending poster fetches
    int                             m_postersInFlight = 0;
};

} // namespace xyz
