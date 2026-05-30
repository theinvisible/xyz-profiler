#include "LibraryController.h"

#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"
#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"
#include "models/MovieListModel.h"
#include "models/MovieSortProxyModel.h"
#include "models/MovieTableModel.h"
#include "models/MovieTreeModel.h"
#include "tmdb/TmdbClient.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPromise>
#include <QScopeGuard>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUrl>
#include <QtConcurrent>

namespace xyz {

LibraryController::LibraryController(QObject* parent)
    : QObject(parent),
      m_listModel(std::make_unique<MovieListModel>()),
      m_tableModel(std::make_unique<MovieTableModel>()),
      m_treeModel(std::make_unique<MovieTreeModel>()),
      m_sortProxy(std::make_unique<MovieSortProxyModel>())
{
    m_sortProxy->setSourceModel(m_listModel.get());

    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::progressValueChanged,
            this, &LibraryController::onImportProgressChanged_);
    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::progressRangeChanged,
            this, &LibraryController::onImportRangeChanged_);
    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::progressTextChanged,
            this, &LibraryController::setImportStage_);
    connect(&m_importWatcher, &QFutureWatcher<ImportOutcome>::finished,
            this, &LibraryController::onImportFinished_);

    connect(&m_parseWatcher, &QFutureWatcher<ParseOutcome>::finished,
            this, &LibraryController::onParseFinished_);

    connect(&m_bulkWriteWatcher, &QFutureWatcher<ImportOutcome>::progressValueChanged,
            this, [this](int v) { m_bulkCurrent = v; emit bulkMatchStateChanged(); });
    connect(&m_bulkWriteWatcher, &QFutureWatcher<ImportOutcome>::progressRangeChanged,
            this, [this](int, int max) { m_bulkTotal = max; emit bulkMatchStateChanged(); });
    connect(&m_bulkWriteWatcher, &QFutureWatcher<ImportOutcome>::finished,
            this, &LibraryController::onBulkWriteFinished_);
}

LibraryController::~LibraryController() = default;

bool LibraryController::libraryOpen() const
{
    return m_db && m_db->isOpen();
}

int LibraryController::movieCount() const
{
    return m_listModel ? m_listModel->rowCount() : 0;
}

void LibraryController::setStatus_(const QString& message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void LibraryController::setMoviesOnBothModels_(QList<Movie> movies)
{
    m_listModel->setMovies(movies);
    m_tableModel->setMovies(movies);
    m_treeModel->setMovies(movies);
}

// ---------------------------------------------------------------------------
// Library
// ---------------------------------------------------------------------------

bool LibraryController::openLibrary(const QString& path)
{
    auto db = std::make_unique<Database>();
    if (!db->open(path)) {
        setStatus_(tr("Failed to open %1: %2").arg(path, db->errorString()));
        return false;
    }
    QString err;
    if (!Migrations::migrateToLatest(*db, &err)) {
        setStatus_(tr("Migration failed: %1").arg(err));
        return false;
    }
    auto repo = std::make_unique<MovieRepository>(*db);
    m_libraryRoot = QFileInfo(path).absolutePath();
    repo->setLibraryRoot(m_libraryRoot);

    m_db          = std::move(db);
    m_repo        = std::move(repo);
    m_libraryPath = path;

    emit libraryChanged();
    refresh();
    setStatus_(tr("Opened library with %1 movies").arg(movieCount()));
    return true;
}

// ---------------------------------------------------------------------------
// Import
// ---------------------------------------------------------------------------

QStringList LibraryController::previewSampleTitles() const
{
    QStringList out;
    const int take = std::min<int>(8, int(m_previewMovies.size()));
    out.reserve(take);
    for (int i = 0; i < take; ++i) {
        const auto& m = m_previewMovies[i];
        if (m.productionYear > 0)
            out << QStringLiteral("%1  (%2)").arg(m.title).arg(m.productionYear);
        else
            out << m.title;
    }
    return out;
}

bool LibraryController::beginImport(const QString& xmlPath, const QString& imagesDir)
{
    if (m_importInProgress) { setStatus_(tr("Import already running")); return false; }
    if (!m_repo)            { setStatus_(tr("No library open"));        return false; }

    m_previewActive = false;
    m_previewMovies.clear();
    m_previewSourceName = QFileInfo(xmlPath).fileName();
    m_previewImagesDir  = imagesDir;
    m_importInProgress  = true;
    m_importStage       = tr("Reading Collection.xml…");
    m_importCurrent     = 0;
    m_importTotal       = 0;
    emit importStateChanged();
    setStatus_(tr("Reading %1…").arg(m_previewSourceName));

    auto task = [xmlPath, imagesDir]() -> ParseOutcome {
        ParseOutcome out;
        DvdProfilerXmlImporter importer(imagesDir);
        auto res = importer.importFile(xmlPath);
        if (!res.ok) { out.errorString = res.errorString; return out; }
        out.movies = std::move(res.movies);
        return out;
    };
    m_parseWatcher.setFuture(QtConcurrent::run(task));
    return true;
}

void LibraryController::onParseFinished_()
{
    const auto outcome = m_parseWatcher.result();
    m_importInProgress = false;
    m_importStage.clear();

    if (!outcome.errorString.isEmpty()) {
        m_previewMovies.clear();
        m_previewActive = false;
        emit importStateChanged();
        setStatus_(tr("Import failed: %1").arg(outcome.errorString));
        emit importFinished(0, outcome.errorString);
        return;
    }
    m_previewMovies = outcome.movies;
    m_previewActive = true;
    emit importStateChanged();
    setStatus_(tr("Ready to import %1 movies — confirm to write").arg(previewMovieCount()));
}

bool LibraryController::commitImport()
{
    if (!m_previewActive || m_previewMovies.isEmpty()) return false;
    if (m_importInProgress) return false;

    QList<Movie> movies = std::move(m_previewMovies);
    m_previewMovies.clear();
    m_previewActive = false;
    runWriter_(std::move(movies), false);
    return true;
}

void LibraryController::cancelImport()
{
    if (!m_previewActive) return;
    m_previewActive = false;
    m_previewMovies.clear();
    m_previewSourceName.clear();
    m_previewImagesDir.clear();
    emit importStateChanged();
    setStatus_(tr("Import cancelled"));
    emit importFinished(0, {});
}

bool LibraryController::importDvdProfilerXml(const QString& xmlPath,
                                             const QString& imagesDir)
{
    if (m_importInProgress) { setStatus_(tr("Import already running")); return false; }
    if (!m_repo) {
        setStatus_(tr("No library open"));
        emit importFinished(0, m_statusMessage);
        return false;
    }
    DvdProfilerXmlImporter importer(imagesDir);
    auto res = importer.importFile(xmlPath);
    if (!res.ok) {
        setStatus_(tr("Import failed: %1").arg(res.errorString));
        emit importFinished(0, res.errorString);
        return false;
    }
    runWriter_(std::move(res.movies), true);
    return true;
}

void LibraryController::runWriter_(QList<Movie> movies, bool /*autoCommit*/)
{
    const QString dbPath      = m_libraryPath;
    const QString libraryRoot = m_libraryRoot;

    auto task = [movies = std::move(movies), dbPath, libraryRoot]
                (QPromise<ImportOutcome>& promise) mutable
    {
        ImportOutcome out;
        Database db;
        if (!db.open(dbPath)) {
            out.errorString = QObject::tr("DB open failed: %1").arg(db.errorString());
            promise.addResult(out);
            return;
        }
        QString migErr;
        if (!Migrations::migrateToLatest(db, &migErr)) {
            out.errorString = QObject::tr("Migration failed: %1").arg(migErr);
            promise.addResult(out);
            return;
        }
        MovieRepository repo(db);
        repo.setLibraryRoot(libraryRoot);

        const int total = int(movies.size());
        promise.setProgressRange(0, total);
        promise.setProgressValueAndText(0,
            QObject::tr("Writing %1 movies to database…").arg(total));

        auto conn = db.handle();
        if (!conn.transaction()) {
            out.errorString = QObject::tr("BEGIN failed: %1").arg(conn.lastError().text());
            promise.addResult(out);
            return;
        }
        for (int i = 0; i < total; ++i) {
            if (promise.isCanceled()) { conn.rollback(); return; }
            if (!repo.insert(movies[i])) {
                conn.rollback();
                out.errorString = repo.lastError();
                promise.addResult(out);
                return;
            }
            promise.setProgressValue(i + 1);
        }
        if (!conn.commit()) {
            out.errorString = QObject::tr("COMMIT failed: %1").arg(conn.lastError().text());
            promise.addResult(out);
            return;
        }
        out.imported = total;
        promise.addResult(out);
    };

    m_importInProgress = true;
    m_importStage      = tr("Writing to database…");
    m_importCurrent    = 0;
    m_importTotal      = 0;
    emit importStateChanged();
    m_importWatcher.setFuture(QtConcurrent::run(task));
}

void LibraryController::onImportProgressChanged_(int current)
{
    m_importCurrent = current;
    emit importStateChanged();
}

void LibraryController::onImportRangeChanged_(int /*min*/, int max)
{
    m_importTotal = max;
    emit importStateChanged();
}

void LibraryController::setImportStage_(const QString& stage)
{
    m_importStage = stage;
    emit importStateChanged();
}

void LibraryController::onImportFinished_()
{
    const auto outcome = m_importWatcher.result();
    m_importInProgress = false;
    m_importStage.clear();
    emit importStateChanged();

    if (!outcome.errorString.isEmpty()) {
        setStatus_(tr("Import failed: %1").arg(outcome.errorString));
        emit importFinished(0, outcome.errorString);
        return;
    }
    setStatus_(tr("Imported %1 movies").arg(outcome.imported));
    refresh();
    emit importFinished(outcome.imported, {});
}

// ---------------------------------------------------------------------------
// Refresh / Search / Selection
// ---------------------------------------------------------------------------

void LibraryController::refresh()
{
    if (!m_repo) return;
    setMoviesOnBothModels_(m_repo->getAll());
    emit moviesChanged();

    if (!m_selectedId.isEmpty())
        selectMovie(m_selectedId);
    else if (!m_listModel->movies().isEmpty())
        selectMovie(m_listModel->movies().first().id);
}

void LibraryController::search(const QString& query)
{
    if (!m_repo) return;
    if (query.trimmed().isEmpty()) { refresh(); return; }
    setMoviesOnBothModels_(m_repo->search(query));
    emit moviesChanged();
    setStatus_(tr("Search '%1': %2 hits").arg(query).arg(movieCount()));
}

void LibraryController::selectMovie(const QString& id)
{
    if (id.isEmpty()) { clearSelection(); return; }
    if (const auto* m = m_listModel->find(id)) {
        m_selected   = *m;
        m_selectedId = id;
        emit selectionChanged();
        return;
    }
    if (m_repo) {
        if (auto opt = m_repo->getById(id); opt.has_value()) {
            m_selected   = *opt;
            m_selectedId = id;
            emit selectionChanged();
            return;
        }
    }
    clearSelection();
}

void LibraryController::clearSelection()
{
    if (m_selectedId.isEmpty()) return;
    m_selected   = {};
    m_selectedId.clear();
    emit selectionChanged();
}

// ---------------------------------------------------------------------------
// TMDb integration
// ---------------------------------------------------------------------------

void LibraryController::setTmdbClient(TmdbClient* client)
{
    if (m_tmdb == client) return;
    if (m_tmdb) m_tmdb->disconnect(this);
    m_tmdb = client;
    if (!m_tmdb) { emit tmdbStateChanged(); return; }

    connect(m_tmdb, &TmdbClient::searchFinished, this,
            [this](const QString&, int, const QList<TmdbCandidate>& hits, const QString& err)
    {
        m_tmdbSearching   = false;
        m_tmdbSearchError = err;
        m_tmdbCandidates  = hits;
        emit tmdbStateChanged();
        if (err.isEmpty())
            setStatus_(tr("TMDb: %1 candidates").arg(hits.size()));
        else
            setStatus_(tr("TMDb search failed: %1").arg(err));
    });

    // Shared movie-detail handler — only acts when an "add title" fetch is
    // pending (m_addingTmdbId != 0); other getMovie() callers are unaffected.
    connect(m_tmdb, &TmdbClient::movieFinished, this,
            [this](const TmdbMovieDetails& d, const QString& err)
    {
        if (m_addingTmdbId == 0) return;
        const int     reqId  = m_addingTmdbId;
        const QString poster = m_addingPosterPath;
        const QString format = m_addingFormat;
        m_addingTmdbId = 0;
        m_addingPosterPath.clear();
        m_addingFormat.clear();

        if (!err.isEmpty()) {
            setStatus_(tr("Could not fetch title from TMDb: %1").arg(err));
            return;
        }
        if (!m_repo) return;

        const QString newId = QStringLiteral("tmdb%1").arg(d.id > 0 ? d.id : reqId);
        if (auto existing = m_repo->getById(newId); existing.has_value()) {
            refresh();
            selectMovie(newId);
            setStatus_(tr("\"%1\" is already in your collection").arg(existing->title));
            return;
        }

        Movie m;
        m.id                 = newId;
        m.title              = d.title;
        m.originalTitle      = d.originalTitle;
        m.overview           = d.overview;
        m.genres             = d.genres;
        m.studios            = d.productionCompanies;
        m.countriesOfOrigin  = d.productionCountries;
        m.runningTimeMinutes = d.runtime;
        m.format             = format;
        m.tmdbId             = d.id;
        m.membership.type    = QStringLiteral("Owned");
        m.profileTimestamp   = QDateTime::currentDateTime();
        m.lastEdited         = m.profileTimestamp;
        if (d.releaseDate.size() >= 4) {
            m.productionYear = d.releaseDate.left(4).toInt();
            m.releaseDate    = QDate::fromString(d.releaseDate, Qt::ISODate);
        }

        if (!m_repo->insert(m)) {
            setStatus_(tr("Failed to add title: %1").arg(m_repo->lastError()));
            return;
        }
        refresh();
        selectMovie(newId);
        setStatus_(tr("Added \"%1\"").arg(m.title));
        if (!poster.isEmpty())
            downloadTmdbPoster_(newId, poster);
    });

    emit tmdbStateChanged();
}

void LibraryController::searchSelectedOnTmdb()
{
    if (!m_tmdb || !m_tmdb->hasApiKey()) {
        setStatus_(tr("TMDb is not configured (set TMDB_API_KEY)"));
        m_tmdbSearchError = tr("TMDb is not configured (set TMDB_API_KEY)");
        m_tmdbCandidates.clear();
        emit tmdbStateChanged();
        return;
    }
    if (m_selectedId.isEmpty()) return;

    m_tmdbSearching      = true;
    m_tmdbSearchError.clear();
    m_tmdbCandidates.clear();
    m_tmdbSearchingForId = m_selectedId;
    emit tmdbStateChanged();
    setStatus_(tr("Searching TMDb for '%1'…").arg(m_selected.title));
    m_tmdb->search(m_selected.title, m_selected.productionYear);
}

void LibraryController::pickTmdbMatch(int tmdbId)
{
    if (!m_repo) return;
    if (m_selectedId.isEmpty() || tmdbId <= 0) return;

    QString posterPath;
    for (const auto& c : m_tmdbCandidates) {
        if (c.id == tmdbId) { posterPath = c.posterPath; break; }
    }

    Movie m = m_selected;
    m.tmdbId = tmdbId;
    if (!m_repo->insert(m)) {
        setStatus_(tr("Failed to persist TMDb match: %1").arg(m_repo->lastError()));
        return;
    }
    m_selected = m;
    m_tmdbCandidates.clear();
    emit selectionChanged();
    emit tmdbStateChanged();
    emit tmdbMatchPicked(m.id, tmdbId);
    setStatus_(tr("Linked to TMDb #%1").arg(tmdbId));

    // Update both models
    const int listIdx = m_listModel->indexOfId(m.id);
    if (listIdx >= 0) {
        QList<Movie> next = m_listModel->movies();
        next[listIdx] = m;
        setMoviesOnBothModels_(std::move(next));
    }

    if (!posterPath.isEmpty() && m_tmdb)
        downloadTmdbPoster_(m.id, posterPath);
}

void LibraryController::updateMovie(const Movie& edited)
{
    if (!m_repo) { setStatus_(tr("No library open")); return; }
    if (edited.id.isEmpty()) return;

    if (!m_repo->insert(edited)) {   // insert() is an upsert by id
        setStatus_(tr("Failed to save changes: %1").arg(m_repo->lastError()));
        return;
    }

    if (m_selectedId == edited.id) {
        m_selected = edited;
        emit selectionChanged();
    }

    // Patch the row in place in the models (cheaper than a full refresh and it
    // keeps scroll position) — same approach as pickTmdbMatch.
    const int listIdx = m_listModel->indexOfId(edited.id);
    if (listIdx >= 0) {
        QList<Movie> next = m_listModel->movies();
        next[listIdx] = edited;
        setMoviesOnBothModels_(std::move(next));
        emit moviesChanged();
    } else {
        refresh();
    }
    setStatus_(tr("Updated \"%1\"").arg(edited.title));
}

void LibraryController::deleteMovie(const QString& id)
{
    if (!m_repo) { setStatus_(tr("No library open")); return; }
    if (id.isEmpty()) return;

    QString title;
    if (const auto* m = m_listModel->find(id)) title = m->title;

    if (!m_repo->remove(id)) {
        setStatus_(tr("Failed to delete: %1").arg(m_repo->lastError()));
        return;
    }

    if (m_selectedId == id) clearSelection();
    refresh();   // reloads all models from the DB and re-selects sensibly
    setStatus_(title.isEmpty() ? tr("Deleted title")
                               : tr("Deleted \"%1\"").arg(title));
}

void LibraryController::addMovieFromTmdb(int tmdbId, const QString& format,
                                         const QString& posterPath)
{
    if (!m_repo) { setStatus_(tr("No library open")); return; }
    if (tmdbId <= 0) return;
    if (!m_tmdb || !m_tmdb->hasApiKey()) {
        setStatus_(tr("TMDb is not configured (set TMDB_API_KEY)"));
        return;
    }
    m_addingTmdbId     = tmdbId;
    m_addingPosterPath = posterPath;
    m_addingFormat     = format;
    setStatus_(tr("Adding title from TMDb…"));
    m_tmdb->getMovie(tmdbId);
}

void LibraryController::downloadTmdbPosterForMovie(const QString& movieId,
                                                   const QString& posterPath)
{
    if (!m_repo) { setStatus_(tr("No library open")); return; }
    if (!m_tmdb || !m_tmdb->hasApiKey()) return;
    if (movieId.isEmpty() || posterPath.isEmpty()) return;
    downloadTmdbPoster_(movieId, posterPath);
}

void LibraryController::clearTmdbCandidates()
{
    if (m_tmdbCandidates.isEmpty() && m_tmdbSearchError.isEmpty()) return;
    m_tmdbCandidates.clear();
    m_tmdbSearchError.clear();
    emit tmdbStateChanged();
}

void LibraryController::downloadTmdbPoster_(const QString& movieId,
                                            const QString& posterPath,
                                            const std::function<void()>& onDone)
{
    const QString url = m_tmdb->imageUrl(posterPath, QStringLiteral("w500"));
    if (url.isEmpty()) { if (onDone) onDone(); return; }

    const QString coversDir = m_libraryRoot + QStringLiteral("/covers");
    QDir().mkpath(coversDir);
    const QString savePath = coversDir + QStringLiteral("/")
                           + movieId + QStringLiteral("f.jpg");

    const QUrl imageUrl(url);
    QNetworkRequest req(imageUrl);
    auto* reply = m_tmdb->network()->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, movieId, savePath, onDone]()
    {
        reply->deleteLater();
        // Run the completion callback (poster-queue pump) on EVERY exit path.
        auto done = qScopeGuard([&] { if (onDone) onDone(); });

        if (reply->error() != QNetworkReply::NoError) {
            setStatus_(tr("Poster download failed: %1").arg(reply->errorString()));
            return;
        }
        const QByteArray data = reply->readAll();
        if (data.isEmpty()) return;

        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            setStatus_(tr("Cannot save poster: %1").arg(file.errorString()));
            return;
        }
        file.write(data);
        file.close();

        if (!m_repo) return;
        // Lightweight single-column update — no full upsert / child re-insert.
        if (!m_repo->setCoverFront(movieId, savePath)) return;

        // The poster was overwritten in place (same path). Tell the UI to drop
        // its cached pixmaps for this path before the repaints below, otherwise
        // the grid, list and detail pane keep showing the previous artwork.
        emit coverUpdated(savePath);

        if (m_selectedId == movieId) {
            m_selected.coverFrontPath = savePath;
            emit selectionChanged();
        }
        const int idx = m_listModel->indexOfId(movieId);
        if (idx >= 0) {
            QList<Movie> next = m_listModel->movies();
            next[idx].coverFrontPath = savePath;
            setMoviesOnBothModels_(std::move(next));
        }
    });
}

// ---------------------------------------------------------------------------
// Bulk TMDb match
// ---------------------------------------------------------------------------

void LibraryController::applyTmdbMatches(const QList<TmdbBulkMatch>& matches,
                                         bool downloadPosters)
{
    if (!m_repo) { setStatus_(tr("No library open")); return; }
    if (matches.isEmpty()) return;
    if (m_bulkInProgress) return;

    // Build the list of movies to write: copy each from the current model and
    // set ONLY the tmdbId. Imported metadata is preserved.
    QHash<QString, int> wantTmdb;
    wantTmdb.reserve(matches.size());
    for (const auto& m : matches)
        if (!m.movieId.isEmpty() && m.tmdbId > 0)
            wantTmdb.insert(m.movieId, m.tmdbId);

    QList<Movie> updated;
    updated.reserve(wantTmdb.size());
    for (const Movie& m : m_listModel->movies()) {
        auto it = wantTmdb.constFind(m.id);
        if (it == wantTmdb.constEnd()) continue;
        Movie copy = m;
        copy.tmdbId = it.value();
        updated.append(std::move(copy));
    }
    if (updated.isEmpty()) return;

    // Queue posters for after the write (if requested).
    m_posterQueue.clear();
    if (downloadPosters) {
        for (const auto& m : matches)
            if (!m.movieId.isEmpty() && m.tmdbId > 0 && !m.posterPath.isEmpty())
                m_posterQueue.append(m);
    }

    // Write on a worker thread (own DB connection), one transaction.
    const QString dbPath      = m_libraryPath;
    const QString libraryRoot = m_libraryRoot;
    auto task = [updated = std::move(updated), dbPath, libraryRoot]
                (QPromise<ImportOutcome>& promise) mutable
    {
        ImportOutcome out;
        Database db;
        if (!db.open(dbPath)) {
            out.errorString = QObject::tr("DB open failed: %1").arg(db.errorString());
            promise.addResult(out);
            return;
        }
        MovieRepository repo(db);
        repo.setLibraryRoot(libraryRoot);

        const int total = int(updated.size());
        promise.setProgressRange(0, total);
        auto conn = db.handle();
        if (!conn.transaction()) {
            out.errorString = QObject::tr("BEGIN failed: %1").arg(conn.lastError().text());
            promise.addResult(out);
            return;
        }
        for (int i = 0; i < total; ++i) {
            if (promise.isCanceled()) { conn.rollback(); return; }
            if (!repo.insert(updated[i])) {
                conn.rollback();
                out.errorString = repo.lastError();
                promise.addResult(out);
                return;
            }
            promise.setProgressValue(i + 1);
        }
        if (!conn.commit()) {
            out.errorString = QObject::tr("COMMIT failed: %1").arg(conn.lastError().text());
            promise.addResult(out);
            return;
        }
        out.imported = total;
        promise.addResult(out);
    };

    m_bulkInProgress = true;
    m_bulkStage   = tr("Linking %1 titles to TMDb…").arg(updated.size());
    m_bulkCurrent = 0;
    m_bulkTotal   = int(updated.size());
    emit bulkMatchStateChanged();
    m_bulkWriteWatcher.setFuture(QtConcurrent::run(task));
}

void LibraryController::onBulkWriteFinished_()
{
    const auto outcome = m_bulkWriteWatcher.result();
    m_bulkInProgress = false;
    m_bulkStage.clear();
    emit bulkMatchStateChanged();

    if (!outcome.errorString.isEmpty()) {
        m_posterQueue.clear();
        setStatus_(tr("Bulk match failed: %1").arg(outcome.errorString));
        emit bulkMatchFinished(0, outcome.errorString);
        return;
    }

    refresh();   // reload all models from the DB (tmdbIds now set)
    setStatus_(tr("Linked %1 titles to TMDb").arg(outcome.imported));
    emit bulkMatchFinished(outcome.imported, {});

    // Now fetch posters (if any were queued), throttled.
    pumpPosterQueue_();
}

void LibraryController::pumpPosterQueue_()
{
    constexpr int kMaxInFlight = 6;
    while (m_postersInFlight < kMaxInFlight && !m_posterQueue.isEmpty()) {
        const TmdbBulkMatch m = m_posterQueue.takeFirst();
        ++m_postersInFlight;
        downloadTmdbPoster_(m.movieId, m.posterPath, [this] {
            --m_postersInFlight;
            pumpPosterQueue_();   // start the next as each completes
            if (m_postersInFlight == 0 && m_posterQueue.isEmpty())
                setStatus_(tr("Posters downloaded"));
        });
    }
}

// ---------------------------------------------------------------------------
// Box-set grouping
// ---------------------------------------------------------------------------

QList<Movie> LibraryController::groupByBoxSet_(QList<Movie> movies)
{
    QHash<QString, int> idToIdx;
    idToIdx.reserve(movies.size());
    for (int i = 0; i < movies.size(); ++i)
        idToIdx.insert(movies[i].id, i);

    QSet<QString> placed;
    placed.reserve(movies.size());
    QList<Movie> result;
    result.reserve(movies.size());

    for (const auto& m : movies) {
        if (placed.contains(m.id)) continue;
        if (!m.boxSet.parentId.isEmpty() && idToIdx.contains(m.boxSet.parentId))
            continue;

        result.append(m);
        placed.insert(m.id);

        if (m.boxSet.isParent) {
            for (const auto& childId : m.boxSet.childIds) {
                if (!placed.contains(childId) && idToIdx.contains(childId)) {
                    result.append(movies[idToIdx.value(childId)]);
                    placed.insert(childId);
                }
            }
        }
    }
    for (const auto& m : movies) {
        if (!placed.contains(m.id))
            result.append(m);
    }
    return result;
}

} // namespace xyz
