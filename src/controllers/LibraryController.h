#pragma once

#include "domain/Movie.h"
#include "models/MovieListModel.h"
#include "tmdb/TmdbTypes.h"

#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <memory>

namespace xyz {

class Database;
class MovieRepository;
class TmdbClient;

// Top-level QML-facing controller for the library.
//
// Owns the database connection, the repository, and the list model exposed
// to QML. QML calls Q_INVOKABLE actions (openLibrary, search, import,
// selectMovie); status/properties drive bindings on the QML side.
//
// The "selected movie" surface is fan-out: when QML calls selectMovie(id)
// we look the movie up in the list model and update every detail-pane
// Q_PROPERTY in one go, emitting a single change signal. This keeps the
// QML detail view as a flat set of bindings without per-field round-trips.
// Note: NOT marked QML_ELEMENT — the application owns the controller
// instance and registers it explicitly via `qmlRegisterSingletonInstance`
// in main(). That ensures QML sees the same instance the C++ side
// configured (with the DB already open, etc.), instead of the engine
// default-constructing a fresh one.
class LibraryController : public QObject {
    Q_OBJECT

    // ---- Library state ----------------------------------------------------
    Q_PROPERTY(QString libraryPath   READ libraryPath   NOTIFY libraryChanged)
    Q_PROPERTY(bool    libraryOpen   READ libraryOpen   NOTIFY libraryChanged)
    Q_PROPERTY(int     movieCount    READ movieCount    NOTIFY moviesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(MovieListModel* movies READ movies CONSTANT)

    // ---- Async import state ----------------------------------------------
    Q_PROPERTY(bool    importInProgress READ importInProgress NOTIFY importStateChanged)
    Q_PROPERTY(QString importStage      READ importStage      NOTIFY importStateChanged)
    Q_PROPERTY(int     importCurrent    READ importCurrent    NOTIFY importStateChanged)
    Q_PROPERTY(int     importTotal      READ importTotal      NOTIFY importStateChanged)

    // ---- Import preview (between parse and DB-write phases) --------------
    Q_PROPERTY(bool        previewActive       READ previewActive       NOTIFY importStateChanged)
    Q_PROPERTY(int         previewMovieCount   READ previewMovieCount   NOTIFY importStateChanged)
    Q_PROPERTY(QStringList previewSampleTitles READ previewSampleTitles NOTIFY importStateChanged)
    Q_PROPERTY(QString     previewSourceName   READ previewSourceName   NOTIFY importStateChanged)
    Q_PROPERTY(QString     previewImagesDir    READ previewImagesDir    NOTIFY importStateChanged)

    // ---- TMDb state ------------------------------------------------------
    Q_PROPERTY(bool         tmdbAvailable     READ tmdbAvailable     CONSTANT)
    Q_PROPERTY(bool         tmdbSearching     READ tmdbSearching     NOTIFY tmdbStateChanged)
    Q_PROPERTY(QString      tmdbSearchError   READ tmdbSearchError   NOTIFY tmdbStateChanged)
    Q_PROPERTY(QVariantList tmdbCandidates    READ tmdbCandidates    NOTIFY tmdbStateChanged)
    Q_PROPERTY(int          selectedTmdbId    READ selectedTmdbId    NOTIFY selectionChanged)

    // ---- Selected movie (detail-pane surface) -----------------------------
    Q_PROPERTY(bool    hasSelection         READ hasSelection         NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedId           READ selectedId           NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedTitle        READ selectedTitle        NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedOriginalTitle READ selectedOriginalTitle NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedSortTitle    READ selectedSortTitle    NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDistTrait    READ selectedDistTrait    NOTIFY selectionChanged)
    Q_PROPERTY(int     selectedYear         READ selectedYear         NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedReleaseDate  READ selectedReleaseDate  NOTIFY selectionChanged)
    Q_PROPERTY(int     selectedRuntime      READ selectedRuntime      NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedFormat       READ selectedFormat       NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedRating       READ selectedRating       NOTIFY selectionChanged)
    Q_PROPERTY(int     selectedRatingAge    READ selectedRatingAge    NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedRatingDetails READ selectedRatingDetails NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCaseType     READ selectedCaseType     NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedAspectRatio  READ selectedAspectRatio  NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedDimensions   READ selectedDimensions   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedOverview     READ selectedOverview     NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedNotes        READ selectedNotes        NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedEasterEggs   READ selectedEasterEggs   NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCoverFrontUrl READ selectedCoverFrontUrl NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedCoverBackUrl READ selectedCoverBackUrl NOTIFY selectionChanged)

    Q_PROPERTY(QStringList   selectedGenres        READ selectedGenres        NOTIFY selectionChanged)
    Q_PROPERTY(QStringList   selectedStudios       READ selectedStudios       NOTIFY selectionChanged)
    Q_PROPERTY(QStringList   selectedRegions       READ selectedRegions       NOTIFY selectionChanged)
    Q_PROPERTY(QStringList   selectedSubtitles     READ selectedSubtitles     NOTIFY selectionChanged)
    Q_PROPERTY(QStringList   selectedFeatures      READ selectedFeatures      NOTIFY selectionChanged)
    Q_PROPERTY(QStringList   selectedTags          READ selectedTags          NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList  selectedActors        READ selectedActors        NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList  selectedCredits       READ selectedCredits       NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList  selectedAudioTracks   READ selectedAudioTracks   NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList  selectedDiscs         READ selectedDiscs         NOTIFY selectionChanged)

    Q_PROPERTY(QString selectedPurchaseDate  READ selectedPurchaseDate  NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPurchasePrice READ selectedPurchasePrice NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedPurchasePlace READ selectedPurchasePlace NOTIFY selectionChanged)
    Q_PROPERTY(bool    selectedIsLoaned      READ selectedIsLoaned      NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedLoanUser      READ selectedLoanUser      NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedLoanDue       READ selectedLoanDue       NOTIFY selectionChanged)

public:
    explicit LibraryController(QObject* parent = nullptr);
    ~LibraryController() override;

    // Wire a TMDb client. Optional — leaving it null disables the
    // Find-on-TMDb actions but the rest of the app keeps working.
    // Non-owning; the application owns the client.
    void setTmdbClient(TmdbClient* client);

    // ---- Property getters ------------------------------------------------
    QString libraryPath()   const { return m_libraryPath; }
    bool    libraryOpen()   const;
    int     movieCount()    const;
    QString statusMessage() const { return m_statusMessage; }
    MovieListModel* movies() const { return m_movies.get(); }

    bool    importInProgress() const { return m_importInProgress; }
    QString importStage()      const { return m_importStage; }
    int     importCurrent()    const { return m_importCurrent; }
    int     importTotal()      const { return m_importTotal; }

    bool        previewActive()       const { return m_previewActive; }
    int         previewMovieCount()   const { return int(m_previewMovies.size()); }
    QStringList previewSampleTitles() const;
    QString     previewSourceName()   const { return m_previewSourceName; }
    QString     previewImagesDir()    const { return m_previewImagesDir; }

    bool         tmdbAvailable()   const { return m_tmdb != nullptr; }
    bool         tmdbSearching()   const { return m_tmdbSearching; }
    QString      tmdbSearchError() const { return m_tmdbSearchError; }
    QVariantList tmdbCandidates()  const;
    int          selectedTmdbId()  const { return m_selected.tmdbId; }

    bool    hasSelection() const          { return !m_selectedId.isEmpty(); }
    QString selectedId() const            { return m_selectedId; }
    QString selectedTitle() const         { return m_selected.title; }
    QString selectedOriginalTitle() const { return m_selected.originalTitle; }
    QString selectedSortTitle() const     { return m_selected.sortTitle; }
    QString selectedDistTrait() const     { return m_selected.distTrait; }
    int     selectedYear() const          { return m_selected.productionYear; }
    QString selectedReleaseDate() const;
    int     selectedRuntime() const       { return m_selected.runningTimeMinutes; }
    QString selectedFormat() const        { return m_selected.format; }
    QString selectedRating() const        { return m_selected.rating.value; }
    int     selectedRatingAge() const     { return m_selected.rating.age; }
    QString selectedRatingDetails() const { return m_selected.rating.details; }
    QString selectedCaseType() const      { return m_selected.caseType; }
    QString selectedAspectRatio() const   { return m_selected.videoFormat.aspectRatio; }
    QString selectedDimensions() const    { return m_selected.videoFormat.dimensions; }
    QString selectedOverview() const      { return m_selected.overview; }
    QString selectedNotes() const         { return m_selected.notes; }
    QString selectedEasterEggs() const    { return m_selected.easterEggs; }
    QString selectedCoverFrontUrl() const;
    QString selectedCoverBackUrl() const;

    QStringList  selectedGenres()      const { return m_selected.genres; }
    QStringList  selectedStudios()     const { return m_selected.studios; }
    QStringList  selectedRegions()     const { return m_selected.regions; }
    QStringList  selectedSubtitles()   const { return m_selected.subtitles; }
    QStringList  selectedFeatures()    const { return m_selected.features; }
    QStringList  selectedTags()        const { return m_selected.tags; }
    QVariantList selectedActors()      const;
    QVariantList selectedCredits()     const;
    QVariantList selectedAudioTracks() const;
    QVariantList selectedDiscs()       const;

    QString selectedPurchaseDate()  const;
    QString selectedPurchasePrice() const;
    QString selectedPurchasePlace() const { return m_selected.purchase.place; }
    bool    selectedIsLoaned()      const { return m_selected.loan.loaned; }
    QString selectedLoanUser() const;
    QString selectedLoanDue() const;

    // ---- QML-facing actions ----------------------------------------------
    Q_INVOKABLE bool openLibrary(const QString& path);

    // Two-phase XML import for the GUI:
    //   beginImport(xml, imagesDir)  → parses in a worker, then exposes
    //                                  previewMovieCount/previewSampleTitles
    //                                  for the preview dialog
    //   commitImport()               → writes the parsed Movies to the DB
    //                                  (this is the slow step; uses the
    //                                  importCurrent/importTotal progress)
    //   cancelImport()               → drops the parsed result, no DB write
    Q_INVOKABLE bool beginImport(const QString& xmlPath,
                                 const QString& imagesDir = {});
    Q_INVOKABLE bool commitImport();
    Q_INVOKABLE void cancelImport();

    // One-shot: convenience for the CLI path. Parses + writes in one call,
    // skipping the preview gate. Returns true if the worker was started.
    Q_INVOKABLE bool importDvdProfilerXml(const QString& xmlPath,
                                          const QString& imagesDir = {});

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void selectMovie(const QString& id);
    Q_INVOKABLE void clearSelection();

    // ---- TMDb actions ----------------------------------------------------
    // Trigger /search/movie with the currently selected movie's title and
    // year. Result lands in the tmdbCandidates property (async).
    Q_INVOKABLE void searchSelectedOnTmdb();
    // Persist the chosen TMDb id onto the currently selected movie.
    Q_INVOKABLE void pickTmdbMatch(int tmdbId);
    // Drop the link without re-searching.
    Q_INVOKABLE void clearTmdbCandidates();

    // Convenience for the QML File-Open dialog: strips the file:// prefix.
    Q_INVOKABLE static QString urlToLocalPath(const QString& url);

signals:
    void libraryChanged();
    void moviesChanged();
    void statusMessageChanged();
    void selectionChanged();
    void importStateChanged();
    void importFinished(int imported, const QString& errorString);
    void tmdbStateChanged();
    void tmdbMatchPicked(const QString& movieId, int tmdbId);

private:
    void setStatus_(const QString& message);

    // Async import support ---------------------------------------------------
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

    std::unique_ptr<Database>         m_db;
    std::unique_ptr<MovieRepository>  m_repo;
    std::unique_ptr<MovieListModel>   m_movies;

    QString m_libraryPath;
    QString m_libraryRoot;     // parent dir of the DB file; cover paths resolve against it
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
    QString                         m_tmdbSearchingForId;   // movie id we're searching for
    QList<TmdbCandidate>            m_tmdbCandidates;
};

} // namespace xyz
