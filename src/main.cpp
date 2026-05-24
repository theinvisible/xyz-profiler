#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"
#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"
#include "tmdb/TmdbClient.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTranslator>

namespace {

// ---------------------------------------------------------------------------
// CLI mode: file → DB → optional FTS5 search dump.
// Kept for batch / scripted use; the GUI is the default surface.
// ---------------------------------------------------------------------------
void dumpDetail(const xyz::Movie& m)
{
    qInfo().noquote() << "";
    qInfo().noquote() << "=== Detail:" << m.title << "===";
    qInfo().noquote() << "  id              :" << m.id;
    qInfo().noquote() << "  year/released   :" << m.productionYear
                      << "/" << m.releaseDate.toString(Qt::ISODate);
    qInfo().noquote() << "  runtime         :" << m.runningTimeMinutes << "min";
    qInfo().noquote() << "  format          :" << m.format;
    qInfo().noquote() << "  audio tracks    :" << m.audioTracks.size();
    qInfo().noquote() << "  discs           :" << m.discs.size();
    qInfo().noquote() << "  rating          :" << m.rating.system
                      << m.rating.value
                      << QStringLiteral("(age %1)").arg(m.rating.age);
    qInfo().noquote() << "  loan            :" << (m.loan.loaned ? "LOANED" : "no");
}

int runCli(const QStringList& args, QCommandLineParser& parser,
           QCommandLineOption& imagesOpt, QCommandLineOption& detailOpt,
           QCommandLineOption& dbOpt,     QCommandLineOption& libraryRootOpt,
           QCommandLineOption& searchOpt)
{
    xyz::DvdProfilerXmlImporter importer(parser.value(imagesOpt));
    const auto res = importer.importFile(args.first());
    if (!res.ok) {
        qCritical().noquote() << "Import failed:" << res.errorString;
        return 2;
    }

    qInfo().noquote()
        << "Loaded" << res.movies.size() << "movies from" << args.first();

    const QString detailId = parser.value(detailOpt);
    if (!detailId.isEmpty()) {
        for (const auto& m : res.movies) {
            if (m.id == detailId) { dumpDetail(m); return 0; }
        }
        qWarning().noquote() << "No movie found with id" << detailId;
        return 3;
    }

    const QString dbPath = parser.value(dbOpt);
    if (dbPath.isEmpty()) return 0;

    xyz::Database db;
    if (!db.open(dbPath)) {
        qCritical().noquote() << "DB open failed:" << db.errorString();
        return 4;
    }
    QString err;
    if (!xyz::Migrations::migrateToLatest(db, &err)) {
        qCritical().noquote() << "Migration failed:" << err;
        return 5;
    }
    xyz::MovieRepository repo(db);
    repo.setLibraryRoot(parser.value(libraryRootOpt));
    if (!repo.bulkInsert(res.movies)) {
        qCritical().noquote() << "Bulk insert failed:" << repo.lastError();
        return 6;
    }
    qInfo().noquote() << "Persisted" << repo.count() << "movies to" << dbPath;

    const QString query = parser.value(searchOpt);
    if (!query.isEmpty()) {
        const auto hits = repo.search(query);
        qInfo().noquote() << "Search '" << query << "':" << hits.size() << "hits";
        for (const auto& h : hits) {
            qInfo().noquote() << " ->" << h.title
                              << QStringLiteral("(%1) [%2]")
                                     .arg(h.productionYear).arg(h.format);
        }
    }
    return 0;
}

// Default library file lives under the per-user AppData directory:
// e.g. C:\Users\<name>\AppData\Local\xyz-profiler\library.db on Windows,
// ~/.local/share/xyz-profiler/library.db on Linux. The directory is
// created on first run.
QString defaultLibraryPath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/library.db");
}

// Per-user cache for network responses (TMDb JSON + poster JPEGs).
QString networkCachePath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + QStringLiteral("/network");
    QDir().mkpath(dir);
    return dir;
}

// Factory the QML engine uses to create a QNetworkAccessManager for
// `Image { source: "https://..." }` and other URL-based loaders. Each
// instance gets its own QNetworkDiskCache rooted at the same on-disk
// directory; QNetworkDiskCache is thread-safe across processes/threads
// sharing a directory.
class CachedNamFactory : public QQmlNetworkAccessManagerFactory {
public:
    explicit CachedNamFactory(QString cacheDir, qint64 maxBytes = 256LL * 1024 * 1024)
        : m_cacheDir(std::move(cacheDir)), m_maxBytes(maxBytes) {}

    QNetworkAccessManager* create(QObject* parent) override
    {
        auto* nam   = new QNetworkAccessManager(parent);
        auto* cache = new QNetworkDiskCache(nam);
        cache->setCacheDirectory(m_cacheDir);
        cache->setMaximumCacheSize(m_maxBytes);
        nam->setCache(cache);
        return nam;
    }

private:
    QString m_cacheDir;
    qint64  m_maxBytes;
};

// ---------------------------------------------------------------------------
// GUI mode: Material-styled QQuickWindow driven by LibraryController.
// ---------------------------------------------------------------------------
int runGui(int argc, char* argv[],
           const QString& libraryOverride,
           const QString& libraryRootOverride)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("xyz-profiler"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));
    QCoreApplication::setOrganizationName(QStringLiteral("xyz-profiler"));

    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    // Pick the best-matching app translation for the system locale; if none
    // matches we silently fall back to the source language (English).
    static QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("xyz-profiler"),
                           QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        QGuiApplication::installTranslator(&appTranslator);
    }
    // Standard Qt dialog strings ("OK"/"Cancel"/…) — load from Qt's
    // shipped translations dir if a matching .qm is there.
    static QTranslator qtTranslator;
    const QString qtTrDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"),
                          QStringLiteral("_"), qtTrDir)) {
        QGuiApplication::installTranslator(&qtTranslator);
    }

    QQuickStyle::setStyle(QStringLiteral("Material"));

    // Shared on-disk cache for TMDb JSON + poster JPEGs. Reusing one
    // directory across the API client and the QML image loader lets
    // posters survive restarts and avoids re-downloading them in the
    // grid + detail view.
    const QString cacheDir = networkCachePath();

    auto* appNam = new QNetworkAccessManager(&app);
    {
        auto* cache = new QNetworkDiskCache(appNam);
        cache->setCacheDirectory(cacheDir);
        cache->setMaximumCacheSize(256LL * 1024 * 1024);
        appNam->setCache(cache);
    }

    // User settings — persisted to AppConfig/xyz-profiler.ini.
    xyz::SettingsController settings;

    // TMDb client — key precedence is settings.tmdbApiKey > TMDB_API_KEY env.
    xyz::TmdbClient tmdbClient(xyz::SettingsController::resolveTmdbApiKey(settings),
                               appNam);
    if (tmdbClient.hasApiKey()) {
        tmdbClient.fetchConfiguration();   // background warm-up
    } else {
        qInfo().noquote()
            << "TMDB_API_KEY not configured — TMDb actions will be disabled.";
    }
    // React to runtime key changes from the Settings dialog.
    QObject::connect(&settings, &xyz::SettingsController::tmdbApiKeyChanged,
                     &tmdbClient, [&]() {
        tmdbClient.setApiKey(xyz::SettingsController::resolveTmdbApiKey(settings));
        if (tmdbClient.hasApiKey()) tmdbClient.fetchConfiguration();
    });

    xyz::LibraryController controller;
    controller.setTmdbClient(&tmdbClient);
    Q_UNUSED(libraryRootOverride);

    // Always open a library on startup — the default lives in AppData and
    // is created on first use. `--db PATH` overrides it for power users.
    const QString libPath = libraryOverride.isEmpty()
                                ? defaultLibraryPath()
                                : libraryOverride;
    controller.openLibrary(libPath);

    // Expose controllers as QML singletons backed by the application-
    // owned instances. Must run before loadFromModule so QML's first
    // binding evaluation sees them already in place.
    qmlRegisterSingletonInstance("xyz.profiler", 1, 0, "LibraryController",
                                 &controller);
    qmlRegisterSingletonInstance("xyz.profiler", 1, 0, "SettingsController",
                                 &settings);

    QQmlApplicationEngine engine;
    // QML engine instantiates a NAM per worker thread; route them all
    // through our cache directory so Image downloads (e.g. TMDb poster
    // thumbnails) persist across runs.
    engine.setNetworkAccessManagerFactory(new CachedNamFactory(cacheDir));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("xyz.profiler"), QStringLiteral("Main"));

    return app.exec();
}

} // namespace

int main(int argc, char* argv[])
{
    // First-pass parse with a temporary QCoreApplication so we can decide
    // GUI vs CLI before instantiating the heavier QGuiApplication. The
    // QCoreApplication is scoped to this block.
    QString xmlInput, dbInput, libraryRootInput, imagesInput,
            detailInput, searchInput;
    bool wantGui = true;

    {
        QCoreApplication probe(argc, argv);
        QCoreApplication::setApplicationName(QStringLiteral("xyz-profiler"));
        QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));

        QCommandLineParser parser;
        parser.setApplicationDescription(
            QStringLiteral("XYZ-Profiler: DVD collection manager"));
        parser.addHelpOption();
        parser.addVersionOption();
        parser.addPositionalArgument(
            QStringLiteral("collection.xml"),
            QStringLiteral("Optional DP4 export to import in CLI mode."));

        QCommandLineOption imagesOpt(
            QStringList{QStringLiteral("i"), QStringLiteral("images")},
            QStringLiteral("Directory of cover images."), QStringLiteral("dir"));
        QCommandLineOption detailOpt(
            QStringList{QStringLiteral("d"), QStringLiteral("detail")},
            QStringLiteral("CLI: dump full detail for the given movie ID."),
            QStringLiteral("id"));
        QCommandLineOption dbOpt(
            QStringList{QStringLiteral("b"), QStringLiteral("db")},
            QStringLiteral("SQLite library file. GUI opens it; CLI imports into it."),
            QStringLiteral("path"));
        QCommandLineOption libraryRootOpt(
            QStringList{QStringLiteral("r"), QStringLiteral("library-root")},
            QStringLiteral("Cover paths stored relative to this directory."),
            QStringLiteral("dir"));
        QCommandLineOption searchOpt(
            QStringList{QStringLiteral("s"), QStringLiteral("search")},
            QStringLiteral("CLI: run an FTS5 search and dump hits."),
            QStringLiteral("query"));

        parser.addOption(imagesOpt);
        parser.addOption(detailOpt);
        parser.addOption(dbOpt);
        parser.addOption(libraryRootOpt);
        parser.addOption(searchOpt);
        parser.process(probe);

        const QStringList args = parser.positionalArguments();
        xmlInput         = args.isEmpty() ? QString() : args.first();
        dbInput          = parser.value(dbOpt);
        libraryRootInput = parser.value(libraryRootOpt);
        imagesInput      = parser.value(imagesOpt);
        detailInput      = parser.value(detailOpt);
        searchInput      = parser.value(searchOpt);

        // CLI mode is triggered by a positional xml arg. Without one, we
        // launch the GUI (which can take an optional --db to pre-open).
        wantGui = xmlInput.isEmpty();

        if (!wantGui) {
            return runCli(args, parser, imagesOpt, detailOpt, dbOpt,
                          libraryRootOpt, searchOpt);
        }
    }

    return runGui(argc, argv, dbInput, libraryRootInput);
}
