#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"
#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"
#include "tmdb/TmdbClient.h"
#include "ui/DarkFusionStyle.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QStandardPaths>
#include <QTranslator>

namespace {

// ---------------------------------------------------------------------------
// CLI mode
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

QString defaultLibraryPath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/library.db");
}

QString networkCachePath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::CacheLocation) + QStringLiteral("/network");
    QDir().mkpath(dir);
    return dir;
}

// ---------------------------------------------------------------------------
// GUI mode
// ---------------------------------------------------------------------------
int runGui(int argc, char* argv[],
           const QString& libraryOverride)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("xyz-profiler"));
    QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STRING));
    QCoreApplication::setOrganizationName(QStringLiteral("xyz-profiler"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    // Translations
    static QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("xyz-profiler"),
                           QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&appTranslator);

    // Qt's own strings (standard dialog buttons: OK/Cancel/Yes/No/Save, …) live
    // in qtbase_<locale>.qm. Try the Qt install's translations dir first, then a
    // translations/ folder next to the executable (where windeployqt copies
    // them for the shipped build). Keep the translators alive for the app's
    // lifetime. qtbase covers the widgets; the legacy "qt" meta-catalog is
    // loaded too when present, harmless otherwise.
    const QStringList qtTrDirs = {
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
    };
    static QTranslator qtBaseTranslator;
    static QTranslator qtMetaTranslator;
    for (const QString& dir : qtTrDirs) {
        if (qtBaseTranslator.load(QLocale(), QStringLiteral("qtbase"),
                                  QStringLiteral("_"), dir)) {
            QApplication::installTranslator(&qtBaseTranslator);
            break;
        }
    }
    for (const QString& dir : qtTrDirs) {
        if (qtMetaTranslator.load(QLocale(), QStringLiteral("qt"),
                                  QStringLiteral("_"), dir)) {
            QApplication::installTranslator(&qtMetaTranslator);
            break;
        }
    }

    // Settings + Theme
    xyz::SettingsController settings;
    xyz::DarkFusionStyle::applyTheme(settings.themeName());
    QObject::connect(&settings, &xyz::SettingsController::themeNameChanged,
                     &app, [&]() { xyz::DarkFusionStyle::applyTheme(settings.themeName()); });

    // Network cache for TMDb
    const QString cacheDir = networkCachePath();
    auto* appNam = new QNetworkAccessManager(&app);
    {
        auto* cache = new QNetworkDiskCache(appNam);
        cache->setCacheDirectory(cacheDir);
        cache->setMaximumCacheSize(256LL * 1024 * 1024);
        appNam->setCache(cache);
    }

    // TMDb client
    xyz::TmdbClient tmdbClient(xyz::SettingsController::resolveTmdbApiKey(settings),
                               appNam);
    if (tmdbClient.hasApiKey())
        tmdbClient.fetchConfiguration();

    QObject::connect(&settings, &xyz::SettingsController::tmdbApiKeyChanged,
                     &tmdbClient, [&]() {
        tmdbClient.setApiKey(xyz::SettingsController::resolveTmdbApiKey(settings));
        if (tmdbClient.hasApiKey()) tmdbClient.fetchConfiguration();
    });

    // Controller
    xyz::LibraryController controller;
    controller.setTmdbClient(&tmdbClient);

    const QString libPath = libraryOverride.isEmpty()
                                ? defaultLibraryPath()
                                : libraryOverride;
    controller.openLibrary(libPath);

    // Main window
    xyz::MainWindow window(&controller, &settings, &tmdbClient);
    window.show();

    return app.exec();
}

} // namespace

int main(int argc, char* argv[])
{
    QString xmlInput, dbInput, libraryRootInput;

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
            QStringLiteral("SQLite library file."),
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

        if (!xmlInput.isEmpty()) {
            return runCli(args, parser, imagesOpt, detailOpt, dbOpt,
                          libraryRootOpt, searchOpt);
        }
    }

    return runGui(argc, argv, dbInput);
}
