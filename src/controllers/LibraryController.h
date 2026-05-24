#pragma once

#include "domain/Movie.h"
#include "models/MovieListModel.h"

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
    Q_INVOKABLE bool importDvdProfilerXml(const QString& xmlPath,
                                          const QString& imagesDir = {});
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void selectMovie(const QString& id);
    Q_INVOKABLE void clearSelection();

    // Convenience for the QML File-Open dialog: strips the file:// prefix.
    Q_INVOKABLE static QString urlToLocalPath(const QString& url);

signals:
    void libraryChanged();
    void moviesChanged();
    void statusMessageChanged();
    void selectionChanged();
    void importStateChanged();
    void importFinished(int imported, const QString& errorString);

private:
    void setStatus_(const QString& message);

    // Async import support ---------------------------------------------------
    struct ImportOutcome {
        int     imported = 0;
        QString errorString;
    };
    void onImportProgressChanged_(int current);
    void onImportRangeChanged_(int min, int max);
    void onImportFinished_();
    void setImportStage_(const QString& stage);

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
};

} // namespace xyz
