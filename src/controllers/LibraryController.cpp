#include "LibraryController.h"

#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"
#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"
#include "models/MovieListModel.h"
#include "tmdb/TmdbClient.h"

#include <QFileInfo>
#include <QPromise>
#include <QSqlDatabase>
#include <QSqlError>
#include <QUrl>
#include <QVariantMap>
#include <QtConcurrent>

namespace xyz {
namespace {

QVariantMap personToMap(const Person& p)
{
    QVariantMap m;
    QStringList nameParts;
    if (!p.firstName.isEmpty())  nameParts << p.firstName;
    if (!p.middleName.isEmpty()) nameParts << p.middleName;
    if (!p.lastName.isEmpty())   nameParts << p.lastName;
    m.insert(QStringLiteral("name"),       nameParts.join(QChar(u' ')));
    m.insert(QStringLiteral("firstName"),  p.firstName);
    m.insert(QStringLiteral("lastName"),   p.lastName);
    m.insert(QStringLiteral("role"),       p.role);
    m.insert(QStringLiteral("creditType"), p.creditType);
    m.insert(QStringLiteral("creditedAs"), p.creditedAs);
    m.insert(QStringLiteral("birthYear"),  p.birthYear);
    m.insert(QStringLiteral("voice"),      p.voice);
    m.insert(QStringLiteral("uncredited"), p.uncredited);
    m.insert(QStringLiteral("puppeteer"),  p.puppeteer);
    return m;
}

QVariantMap audioTrackToMap(const AudioTrack& a)
{
    return {
        {QStringLiteral("content"),  a.content},
        {QStringLiteral("format"),   a.format},
        {QStringLiteral("channels"), a.channels},
    };
}

QVariantMap discToMap(const Disc& d)
{
    return {
        {QStringLiteral("descriptionSideA"), d.descriptionSideA},
        {QStringLiteral("descriptionSideB"), d.descriptionSideB},
        {QStringLiteral("discIdSideA"),      d.discIdSideA},
        {QStringLiteral("discIdSideB"),      d.discIdSideB},
        {QStringLiteral("labelSideA"),       d.labelSideA},
        {QStringLiteral("labelSideB"),       d.labelSideB},
        {QStringLiteral("dualLayeredSideA"), d.dualLayeredSideA},
        {QStringLiteral("dualLayeredSideB"), d.dualLayeredSideB},
        {QStringLiteral("dualSided"),        d.dualSided},
        {QStringLiteral("location"),         d.location},
        {QStringLiteral("slot"),             d.slot},
    };
}

QString toFileUrl(const QString& path)
{
    if (path.isEmpty()) return {};
    return QUrl::fromLocalFile(path).toString();
}

} // namespace

LibraryController::LibraryController(QObject* parent)
    : QObject(parent),
      m_movies(std::make_unique<MovieListModel>())
{
    // QFutureWatcher emits these on the thread that owns the watcher —
    // i.e. this object's thread, which is the UI/main thread. So forwarding
    // them straight to Q_PROPERTY notify signals is safe.
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
}

QStringList LibraryController::previewSampleTitles() const
{
    QStringList out;
    const int take = std::min<int>(8, int(m_previewMovies.size()));
    out.reserve(take);
    for (int i = 0; i < take; ++i) {
        const auto& m = m_previewMovies[i];
        if (m.productionYear > 0) {
            out << QStringLiteral("%1  (%2)").arg(m.title).arg(m.productionYear);
        } else {
            out << m.title;
        }
    }
    return out;
}

LibraryController::~LibraryController() = default;

bool LibraryController::libraryOpen() const
{
    return m_db && m_db->isOpen();
}

int LibraryController::movieCount() const
{
    return m_movies ? m_movies->rowCount() : 0;
}

void LibraryController::setStatus_(const QString& message)
{
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

bool LibraryController::openLibrary(const QString& path)
{
    const QString local = urlToLocalPath(path);

    auto db   = std::make_unique<Database>();
    if (!db->open(local)) {
        setStatus_(tr("Failed to open %1: %2").arg(local, db->errorString()));
        return false;
    }
    QString err;
    if (!Migrations::migrateToLatest(*db, &err)) {
        setStatus_(tr("Migration failed: %1").arg(err));
        return false;
    }
    auto repo = std::make_unique<MovieRepository>(*db);

    m_libraryRoot = QFileInfo(local).absolutePath();
    repo->setLibraryRoot(m_libraryRoot);

    m_db          = std::move(db);
    m_repo        = std::move(repo);
    m_libraryPath = local;

    emit libraryChanged();
    refresh();
    setStatus_(tr("Opened library with %1 movies").arg(movieCount()));
    return true;
}

bool LibraryController::beginImport(const QString& xmlPath, const QString& imagesDir)
{
    if (m_importInProgress) {
        setStatus_(tr("Import already running"));
        return false;
    }
    if (!m_repo) {
        setStatus_(tr("No library open"));
        return false;
    }

    const QString localXml    = urlToLocalPath(xmlPath);
    const QString localImages = urlToLocalPath(imagesDir);

    // Reset any leftover preview from a cancelled prior attempt.
    m_previewActive = false;
    m_previewMovies.clear();
    m_previewSourceName = QFileInfo(localXml).fileName();
    m_previewImagesDir  = localImages;
    m_importInProgress  = true;
    m_importStage       = tr("Reading Collection.xml…");
    m_importCurrent     = 0;
    m_importTotal       = 0;
    emit importStateChanged();
    setStatus_(tr("Reading %1…").arg(m_previewSourceName));

    // Parse runs on a worker thread; we don't touch the DB yet.
    auto task = [localXml, localImages]() -> ParseOutcome {
        ParseOutcome out;
        DvdProfilerXmlImporter importer(localImages);
        auto res = importer.importFile(localXml);
        if (!res.ok) {
            out.errorString = res.errorString;
            return out;
        }
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
    // Status fields are reset by runWriter_.
    runWriter_(std::move(movies), /*autoCommit=*/false);
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
    if (m_importInProgress) {
        setStatus_(tr("Import already running"));
        return false;
    }
    if (!m_repo) {
        setStatus_(tr("No library open"));
        emit importFinished(0, m_statusMessage);
        return false;
    }

    const QString localXml    = urlToLocalPath(xmlPath);
    const QString localImages = urlToLocalPath(imagesDir);
    DvdProfilerXmlImporter importer(localImages);
    auto res = importer.importFile(localXml);
    if (!res.ok) {
        setStatus_(tr("Import failed: %1").arg(res.errorString));
        emit importFinished(0, res.errorString);
        return false;
    }
    runWriter_(std::move(res.movies), /*autoCommit=*/true);
    return true;
}

void LibraryController::runWriter_(QList<Movie> movies, bool /*autoCommit*/)
{
    const QString dbPath      = m_libraryPath;
    const QString libraryRoot = m_libraryRoot;

    // Capture by value — the worker runs on a different thread.
    auto task = [movies = std::move(movies), dbPath, libraryRoot]
                (QPromise<ImportOutcome>& promise) mutable
    {
        ImportOutcome out;

        // Open a worker-thread DB connection. SQLite's Qt driver is
        // one-connection-per-thread; do NOT touch the main thread's
        // Database/Repository here.
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

void LibraryController::refresh()
{
    if (!m_repo) return;
    m_movies->setMovies(m_repo->getAll());
    emit moviesChanged();

    // Re-resolve current selection (if still present) so the detail
    // view picks up edits / re-imports. If nothing is selected yet,
    // auto-pick the first row so the detail pane has something to show.
    if (!m_selectedId.isEmpty()) {
        selectMovie(m_selectedId);
    } else if (!m_movies->movies().isEmpty()) {
        selectMovie(m_movies->movies().first().id);
    }
}

void LibraryController::search(const QString& query)
{
    if (!m_repo) return;
    if (query.trimmed().isEmpty()) {
        refresh();
        return;
    }
    m_movies->setMovies(m_repo->search(query));
    emit moviesChanged();
    setStatus_(tr("Search '%1': %2 hits").arg(query).arg(movieCount()));
}

void LibraryController::selectMovie(const QString& id)
{
    if (!m_movies) return;
    if (id.isEmpty()) { clearSelection(); return; }

    if (const auto* m = m_movies->find(id)) {
        m_selected   = *m;
        m_selectedId = id;
        emit selectionChanged();
        return;
    }
    // Fall back to a DB read — useful for direct-detail links that aren't
    // in the current filtered model.
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

QString LibraryController::urlToLocalPath(const QString& url)
{
    if (url.startsWith(QLatin1String("file:"))) return QUrl(url).toLocalFile();
    return url;
}

QString LibraryController::selectedReleaseDate() const
{
    return m_selected.releaseDate.isValid()
               ? m_selected.releaseDate.toString(Qt::ISODate)
               : QString();
}

QString LibraryController::selectedCoverFrontUrl() const
{
    return toFileUrl(m_selected.coverFrontPath);
}

QString LibraryController::selectedCoverBackUrl() const
{
    return toFileUrl(m_selected.coverBackPath);
}

QVariantList LibraryController::selectedActors() const
{
    QVariantList out;
    out.reserve(m_selected.actors.size());
    for (const auto& p : m_selected.actors) out << personToMap(p);
    return out;
}

QVariantList LibraryController::selectedCredits() const
{
    QVariantList out;
    out.reserve(m_selected.credits.size());
    for (const auto& p : m_selected.credits) out << personToMap(p);
    return out;
}

QVariantList LibraryController::selectedAudioTracks() const
{
    QVariantList out;
    out.reserve(m_selected.audioTracks.size());
    for (const auto& a : m_selected.audioTracks) out << audioTrackToMap(a);
    return out;
}

QVariantList LibraryController::selectedDiscs() const
{
    QVariantList out;
    out.reserve(m_selected.discs.size());
    for (const auto& d : m_selected.discs) out << discToMap(d);
    return out;
}

QString LibraryController::selectedPurchaseDate() const
{
    return m_selected.purchase.date.isValid()
               ? m_selected.purchase.date.toString(Qt::ISODate)
               : QString();
}

QString LibraryController::selectedPurchasePrice() const
{
    const auto& p = m_selected.purchase.price;
    if (p.value.isEmpty() || p.value == QLatin1String("0")) return {};
    if (!p.formattedValue.isEmpty()) return p.formattedValue;
    return p.denominationType.isEmpty()
               ? p.value
               : QStringLiteral("%1 %2").arg(p.value, p.denominationType);
}

QString LibraryController::selectedLoanUser() const
{
    QStringList parts;
    if (!m_selected.loan.userFirstName.isEmpty()) parts << m_selected.loan.userFirstName;
    if (!m_selected.loan.userLastName.isEmpty())  parts << m_selected.loan.userLastName;
    return parts.join(QChar(u' '));
}

QString LibraryController::selectedLoanDue() const
{
    return m_selected.loan.due.isValid()
               ? m_selected.loan.due.toString(Qt::ISODate)
               : QString();
}

// ---------------------------------------------------------------------------
// TMDb integration
// ---------------------------------------------------------------------------

void LibraryController::setTmdbClient(TmdbClient* client)
{
    if (m_tmdb == client) return;
    if (m_tmdb) m_tmdb->disconnect(this);
    m_tmdb = client;
    if (!m_tmdb) {
        emit tmdbStateChanged();
        return;
    }
    connect(m_tmdb, &TmdbClient::searchFinished, this,
            [this](const QString&, int, const QList<TmdbCandidate>& hits, const QString& err)
    {
        m_tmdbSearching   = false;
        m_tmdbSearchError = err;
        m_tmdbCandidates  = hits;
        emit tmdbStateChanged();
        if (err.isEmpty()) {
            setStatus_(tr("TMDb: %1 candidates").arg(hits.size()));
        } else {
            setStatus_(tr("TMDb search failed: %1").arg(err));
        }
    });
    emit tmdbStateChanged();
}

QVariantList LibraryController::tmdbCandidates() const
{
    QVariantList out;
    out.reserve(m_tmdbCandidates.size());
    for (const auto& c : m_tmdbCandidates) {
        QVariantMap m;
        m.insert(QStringLiteral("id"),            c.id);
        m.insert(QStringLiteral("title"),         c.title);
        m.insert(QStringLiteral("originalTitle"), c.originalTitle);
        m.insert(QStringLiteral("year"),          c.year());
        m.insert(QStringLiteral("releaseDate"),   c.releaseDate);
        m.insert(QStringLiteral("overview"),      c.overview);
        m.insert(QStringLiteral("voteAverage"),   c.voteAverage);
        m.insert(QStringLiteral("voteCount"),     c.voteCount);
        m.insert(QStringLiteral("posterUrl"),
                 m_tmdb ? m_tmdb->imageUrl(c.posterPath, QStringLiteral("w185"))
                        : QString());
        out << m;
    }
    return out;
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
    // Refresh the model so the selection sticks even when the cached list
    // would otherwise hand back a stale Movie on the next selectMovie().
    if (m_movies) {
        const int idx = m_movies->indexOfId(m.id);
        if (idx >= 0) {
            // Replace in-place via setMovies — cheap for the controller
            // since QList shares its data implicitly.
            QList<Movie> next = m_movies->movies();
            next[idx] = m;
            m_movies->setMovies(std::move(next));
        }
    }
}

void LibraryController::clearTmdbCandidates()
{
    if (m_tmdbCandidates.isEmpty() && m_tmdbSearchError.isEmpty()) return;
    m_tmdbCandidates.clear();
    m_tmdbSearchError.clear();
    emit tmdbStateChanged();
}

} // namespace xyz
