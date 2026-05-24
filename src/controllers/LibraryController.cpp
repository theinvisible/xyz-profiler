#include "LibraryController.h"

#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"
#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"
#include "models/MovieListModel.h"

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
    const QString dbPath      = m_libraryPath;
    const QString libraryRoot = m_libraryRoot;

    // Snapshot of state for the worker — all captured by value so the
    // worker doesn't reach back into LibraryController from another thread.
    auto task = [localXml, localImages, dbPath, libraryRoot](QPromise<ImportOutcome>& promise)
    {
        ImportOutcome out;

        // Phase 1: read XML --------------------------------------------------
        promise.setProgressValueAndText(0, QObject::tr("Reading Collection.xml…"));
        DvdProfilerXmlImporter importer(localImages);
        auto res = importer.importFile(localXml);
        if (!res.ok) {
            out.errorString = res.errorString;
            promise.addResult(out);
            return;
        }
        if (promise.isCanceled()) return;

        // Phase 2: open a *worker-thread* DB connection ----------------------
        // SQLite's Qt driver is one-connection-per-thread. We must NOT touch
        // the main thread's Database/Repository here.
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

        // Phase 3: insert with per-row progress -----------------------------
        const int total = int(res.movies.size());
        promise.setProgressRange(0, total);
        promise.setProgressValueAndText(0, QObject::tr("Writing %1 movies to database…").arg(total));

        auto conn = db.handle();
        if (!conn.transaction()) {
            out.errorString = QObject::tr("BEGIN failed: %1").arg(conn.lastError().text());
            promise.addResult(out);
            return;
        }
        for (int i = 0; i < total; ++i) {
            if (promise.isCanceled()) { conn.rollback(); return; }
            if (!repo.insert(res.movies[i])) {
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
    m_importStage      = tr("Starting import…");
    m_importCurrent    = 0;
    m_importTotal      = 0;
    emit importStateChanged();
    setStatus_(tr("Importing %1…").arg(QFileInfo(localXml).fileName()));

    auto future = QtConcurrent::run(task);
    m_importWatcher.setFuture(future);
    return true;
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

} // namespace xyz
