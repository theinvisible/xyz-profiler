#include "db/Database.h"
#include "db/Migrations.h"
#include "db/MovieRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using xyz::Database;
using xyz::Migrations;
using xyz::Movie;
using xyz::MovieRepository;

namespace {

// Build a small movie carrying at least one value in every child-table-
// backed field, so round-trip tests cover the whole insert/load path.
Movie makeRichMovie(const QString& id = QStringLiteral("085391163926"))
{
    Movie m;
    m.id                              = id;
    m.title                           = QStringLiteral("The Matrix");
    m.originalTitle                   = QStringLiteral("The Matrix");
    m.sortTitle                       = QStringLiteral("Matrix, The");
    m.upc                             = QStringLiteral("0-85391-16392-6");
    m.idMetadata.base                 = QStringLiteral("085391163926");
    m.idMetadata.variantNum           = 0;
    m.idMetadata.localityId           = 0;
    m.idMetadata.localityDescription  = QStringLiteral("United States");
    m.idMetadata.type                 = QStringLiteral("UPCEAN");
    m.distTrait                       = QStringLiteral("Steelbook Edition");
    m.productionYear                  = 1999;
    m.releaseDate                     = QDate(1999, 9, 21);
    m.runningTimeMinutes              = 136;
    m.format                          = QStringLiteral("BluRay");
    m.overview                        = QStringLiteral("A computer hacker learns about reality.");
    m.notes                           = QStringLiteral("Limited steelbook");
    m.easterEggs                      = QStringLiteral("Hidden featurette in menu");
    m.caseType                        = QStringLiteral("HD Slim");
    m.caseSlipCover                   = true;
    m.membership.type                 = QStringLiteral("Owned");
    m.membership.isPartOfOwnedCollection = true;
    m.collectionNumber                = 42;
    m.countAs                         = 1;
    m.wishPriority                    = 0;
    m.locationId                      = QStringLiteral("Shelf-A1");
    m.coverFrontPath                  = QStringLiteral("/covers/085391163926f.jpg");
    m.coverBackPath                   = QStringLiteral("/covers/085391163926b.jpg");

    m.genres            = {QStringLiteral("Action"), QStringLiteral("Science Fiction")};
    m.tags              = {QStringLiteral("watched"), QStringLiteral("favourite")};
    m.studios           = {QStringLiteral("Warner Bros.")};
    m.mediaCompanies    = {QStringLiteral("Warner Home Video")};
    m.subtitles         = {QStringLiteral("English"), QStringLiteral("German")};
    m.countriesOfOrigin = {QStringLiteral("United States")};
    m.regions           = {QStringLiteral("A"), QStringLiteral("B")};
    m.features          = {QStringLiteral("SceneAccess"), QStringLiteral("Commentary")};
    m.lockedFields      = {QStringLiteral("SRP")};

    xyz::Person actor;
    actor.firstName  = QStringLiteral("Keanu");
    actor.lastName   = QStringLiteral("Reeves");
    actor.birthYear  = 1964;
    actor.role       = QStringLiteral("Neo");
    m.actors << actor;

    xyz::Person credit;
    credit.firstName  = QStringLiteral("Lana");
    credit.lastName   = QStringLiteral("Wachowski");
    credit.creditType = QStringLiteral("Direction");
    credit.role       = QStringLiteral("Director");
    m.credits << credit;

    xyz::AudioTrack t;
    t.content  = QStringLiteral("English");
    t.format   = QStringLiteral("DTS-HD MA");
    t.channels = QStringLiteral("5.1");
    m.audioTracks << t;

    xyz::Disc d;
    d.descriptionSideA = QStringLiteral("Main Feature");
    d.discIdSideA      = QStringLiteral("DEADBEEF");
    d.labelSideA       = QStringLiteral("MATRIX");
    d.dualLayeredSideA = true;
    m.discs << d;

    xyz::CustomField cf;
    cf.name  = QStringLiteral("Shelf");
    cf.value = QStringLiteral("A-1");
    m.customFields << cf;

    xyz::Event ev;
    ev.type          = QStringLiteral("Borrowed");
    ev.timestamp     = QDateTime(QDate(2025, 1, 18), QTime(19, 9, 49), QTimeZone::UTC);
    ev.note          = QStringLiteral("Lent to David");
    ev.userFirstName = QStringLiteral("David");
    m.events << ev;

    m.boxSet.parentId = QStringLiteral("parent-id");
    m.boxSet.childIds = {QStringLiteral("c1"), QStringLiteral("c2")};
    m.boxSet.isParent = true;

    m.purchase.price.value                   = QStringLiteral("14.99");
    m.purchase.price.denominationType        = QStringLiteral("EUR");
    m.purchase.price.denominationDescription = QStringLiteral("Europe (Euro)");
    m.purchase.price.formattedValue          = QStringLiteral("14,99 EUR");
    m.purchase.place                         = QStringLiteral("Saturn");
    m.purchase.date                          = QDate(2004, 5, 12);

    m.srp.value             = QStringLiteral("19.99");
    m.srp.denominationType  = QStringLiteral("USD");

    m.rating.system   = QStringLiteral("Film");
    m.rating.value    = QStringLiteral("R");
    m.rating.age      = 18;

    m.videoFormat.aspectRatio   = QStringLiteral("2.40");
    m.videoFormat.videoStandard = QStringLiteral("NTSC");
    m.videoFormat.colorMode     = QStringLiteral("Color");
    m.videoFormat.dimensions    = QStringLiteral("2D");
    m.videoFormat.letterBox     = true;
    m.videoFormat.dualLayered   = true;

    m.loan.loaned        = true;
    m.loan.due           = QDate(2025, 2, 1);
    m.loan.userFirstName = QStringLiteral("David");

    m.review.film = 9;
    m.review.video = 8;

    m.mediaBanners.front = QStringLiteral("Automatic");
    m.mediaBanners.back  = QStringLiteral("Off");

    m.profileTimestamp = QDateTime(QDate(2012, 7, 22), QTime(4, 38, 8), QTimeZone::UTC);
    m.lastEdited       = QDateTime(QDate(2024, 9, 8), QTime(20, 37, 50), QTimeZone::UTC);

    m.tmdbId = 603; // The Matrix

    return m;
}

} // namespace

class TestMovieRepository : public QObject {
    Q_OBJECT

private slots:
    void migrations_create_schema_on_empty_db();
    void migrations_are_idempotent();
    void insert_and_get_by_id_roundtrips();
    void bulk_insert_count_matches();
    void search_matches_title();
    void search_matches_actor_full_name();
    void search_matches_studio();
    void replaces_on_duplicate_id();
    void cover_paths_stored_relative_to_library_root();
};

void TestMovieRepository::migrations_create_schema_on_empty_db()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QCOMPARE(Migrations::currentVersion(db), 0);

    QString err;
    QVERIFY2(Migrations::migrateToLatest(db, &err), qPrintable(err));
    QCOMPARE(Migrations::currentVersion(db), Migrations::latestVersion());
}

void TestMovieRepository::migrations_are_idempotent()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));
    const int v = Migrations::currentVersion(db);

    // Re-run — should not error and version should be unchanged.
    QVERIFY(Migrations::migrateToLatest(db));
    QCOMPARE(Migrations::currentVersion(db), v);
}

void TestMovieRepository::insert_and_get_by_id_roundtrips()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    const auto in = makeRichMovie();
    QVERIFY2(repo.insert(in), qPrintable(repo.lastError()));

    const auto out = repo.getById(in.id);
    QVERIFY(out.has_value());

    QCOMPARE(out->title,            in.title);
    QCOMPARE(out->productionYear,   in.productionYear);
    QCOMPARE(out->releaseDate,      in.releaseDate);
    QCOMPARE(out->format,           in.format);
    QCOMPARE(out->upc,              in.upc);
    QCOMPARE(out->distTrait,        in.distTrait);
    QCOMPARE(out->caseType,         in.caseType);
    QVERIFY (out->caseSlipCover);
    QCOMPARE(out->collectionNumber, in.collectionNumber);

    QCOMPARE(out->genres,           in.genres);
    QCOMPARE(out->tags,             in.tags);
    QCOMPARE(out->studios,          in.studios);
    QCOMPARE(out->subtitles,        in.subtitles);
    QCOMPARE(out->regions,          in.regions);
    QCOMPARE(out->features,         in.features);
    QCOMPARE(out->lockedFields,     in.lockedFields);

    QCOMPARE(out->actors.size(),    1);
    QCOMPARE(out->actors[0].lastName,  QStringLiteral("Reeves"));
    QCOMPARE(out->actors[0].birthYear, 1964);
    QCOMPARE(out->credits.size(),   1);
    QCOMPARE(out->credits[0].creditType, QStringLiteral("Direction"));
    QCOMPARE(out->audioTracks.size(), 1);
    QCOMPARE(out->audioTracks[0].format, QStringLiteral("DTS-HD MA"));
    QCOMPARE(out->discs.size(),     1);
    QVERIFY (out->discs[0].dualLayeredSideA);
    QCOMPARE(out->customFields.size(), 1);
    QCOMPARE(out->events.size(),    1);
    QCOMPARE(out->events[0].type,   QStringLiteral("Borrowed"));

    QCOMPARE(out->boxSet.parentId, in.boxSet.parentId);
    QCOMPARE(out->boxSet.childIds, in.boxSet.childIds);
    QVERIFY (out->boxSet.isParent);

    QCOMPARE(out->purchase.price.value,            in.purchase.price.value);
    QCOMPARE(out->purchase.price.denominationType, in.purchase.price.denominationType);
    QCOMPARE(out->purchase.date,                   in.purchase.date);

    QCOMPARE(out->srp.value, in.srp.value);
    QCOMPARE(out->rating.value, in.rating.value);
    QCOMPARE(out->rating.age,   in.rating.age);

    QCOMPARE(out->videoFormat.aspectRatio, in.videoFormat.aspectRatio);
    QCOMPARE(out->videoFormat.colorMode,   in.videoFormat.colorMode);
    QVERIFY (out->videoFormat.letterBox);

    QVERIFY (out->loan.loaned);
    QCOMPARE(out->loan.due,           in.loan.due);
    QCOMPARE(out->loan.userFirstName, in.loan.userFirstName);

    QCOMPARE(out->review.film,    in.review.film);
    QCOMPARE(out->mediaBanners.front, in.mediaBanners.front);

    QCOMPARE(out->tmdbId, 603);
}

void TestMovieRepository::bulk_insert_count_matches()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    QList<Movie> movies;
    for (int i = 0; i < 5; ++i) {
        auto m = makeRichMovie(QStringLiteral("id-%1").arg(i));
        m.title = QStringLiteral("Title %1").arg(i);
        movies << m;
    }
    QVERIFY2(repo.bulkInsert(movies), qPrintable(repo.lastError()));
    QCOMPARE(repo.count(), 5);
    QCOMPARE(repo.getAll().size(), 5);
}

void TestMovieRepository::search_matches_title()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    repo.bulkInsert({makeRichMovie(QStringLiteral("1"))});

    const auto hits = repo.search(QStringLiteral("matrix"));
    QCOMPARE(hits.size(), 1);
    QCOMPARE(hits[0].title, QStringLiteral("The Matrix"));
}

void TestMovieRepository::search_matches_actor_full_name()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    repo.bulkInsert({makeRichMovie(QStringLiteral("1"))});

    QCOMPARE(repo.search(QStringLiteral("keanu reeves")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("reeves")).size(),       1);
}

void TestMovieRepository::search_matches_studio()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    repo.bulkInsert({makeRichMovie(QStringLiteral("1"))});

    QCOMPARE(repo.search(QStringLiteral("Warner")).size(), 1);
}

void TestMovieRepository::replaces_on_duplicate_id()
{
    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    auto m = makeRichMovie();
    QVERIFY(repo.insert(m));

    m.title = QStringLiteral("Renamed Matrix");
    m.genres = {QStringLiteral("Cyberpunk")};
    QVERIFY(repo.insert(m));

    QCOMPARE(repo.count(), 1);
    const auto out = repo.getById(m.id);
    QVERIFY(out.has_value());
    QCOMPARE(out->title,  QStringLiteral("Renamed Matrix"));
    QCOMPARE(out->genres, QStringList({QStringLiteral("Cyberpunk")}));

    // FTS should reflect the new title, not the old one.
    QCOMPARE(repo.search(QStringLiteral("renamed")).size(), 1);
    QCOMPARE(repo.search(QStringLiteral("matrix")).size(),  1);
}

void TestMovieRepository::cover_paths_stored_relative_to_library_root()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    Database db;
    QVERIFY(db.open(QStringLiteral(":memory:")));
    QVERIFY(Migrations::migrateToLatest(db));

    MovieRepository repo(db);
    repo.setLibraryRoot(tmp.path());

    auto m = makeRichMovie();
    m.coverFrontPath = tmp.path() + QStringLiteral("/Images/matrixf.jpg");
    m.coverBackPath  = tmp.path() + QStringLiteral("/Images/matrixb.jpg");
    QVERIFY(repo.insert(m));

    // Round-trip: read back should reconstruct the absolute path.
    const auto out = repo.getById(m.id);
    QVERIFY(out.has_value());
    QCOMPARE(out->coverFrontPath, m.coverFrontPath);
    QCOMPARE(out->coverBackPath,  m.coverBackPath);

    // And the on-disk row should hold the *relative* form.
    auto conn = db.handle();
    QSqlQuery q(conn);
    q.prepare(QStringLiteral("SELECT cover_front_path FROM movies WHERE id = ?"));
    q.addBindValue(m.id);
    QVERIFY(q.exec());
    QVERIFY(q.next());
    const QString stored = q.value(0).toString();
    QVERIFY2(!QDir::isAbsolutePath(stored), qPrintable(stored));
    QCOMPARE(stored, QStringLiteral("Images/matrixf.jpg"));
}

QTEST_GUILESS_MAIN(TestMovieRepository)
#include "test_movie_repository.moc"
