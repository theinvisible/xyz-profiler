#include "MovieRepository.h"

#include "Database.h"

#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace xyz {
namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Convenience wrappers — QSqlQuery::bindValue with a QVariant that converts
// from primitive types and dates to text/integer reliably.
QString isoOrEmpty(const QDate& d)
{
    return d.isValid() ? d.toString(Qt::ISODate) : QString();
}
QString isoOrEmpty(const QDateTime& dt)
{
    return dt.isValid() ? dt.toString(Qt::ISODate) : QString();
}

// Concatenate the printable name of a Person for use in FTS5 indexing.
QString fullName(const Person& p)
{
    QStringList parts;
    if (!p.firstName.isEmpty())  parts << p.firstName;
    if (!p.middleName.isEmpty()) parts << p.middleName;
    if (!p.lastName.isEmpty())   parts << p.lastName;
    return parts.join(QChar(u' '));
}

QString joinPeople(const QList<Person>& people)
{
    QStringList parts;
    parts.reserve(people.size());
    for (const auto& p : people) parts << fullName(p);
    return parts.join(QChar(u'\n'));
}

// Wipe child rows belonging to a movie. Foreign-key cascade handles this on
// row delete, but for upserts we delete-then-reinsert children to keep the
// schema declarative.
bool deleteChildren(QSqlQuery& q, const QString& movieId)
{
    static const char* kTables[] = {
        "movie_genres", "movie_tags", "movie_studios", "movie_media_companies",
        "movie_subtitles", "movie_countries", "movie_regions", "movie_features",
        "movie_locked_fields", "movie_box_set_children",
        "movie_actors", "movie_credits", "movie_audio_tracks",
        "movie_discs", "movie_custom_fields", "movie_events",
    };
    for (const char* t : kTables) {
        q.prepare(QStringLiteral("DELETE FROM %1 WHERE movie_id = ?").arg(QLatin1String(t)));
        q.addBindValue(movieId);
        if (!q.exec()) return false;
    }
    q.prepare(QStringLiteral("DELETE FROM movies_fts WHERE movie_id = ?"));
    q.addBindValue(movieId);
    return q.exec();
}

bool insertSimpleList(QSqlQuery& q, const QString& table,
                      const QString& movieId, const QStringList& values)
{
    if (values.isEmpty()) return true;
    q.prepare(QStringLiteral(
        "INSERT INTO %1 (movie_id, position, value) VALUES (?, ?, ?)").arg(table));
    for (int i = 0; i < values.size(); ++i) {
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, values[i]);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertBoxSetChildren(QSqlQuery& q, const QString& movieId,
                          const QStringList& childIds)
{
    if (childIds.isEmpty()) return true;
    q.prepare(QStringLiteral(
        "INSERT INTO movie_box_set_children (movie_id, position, child_id) "
        "VALUES (?, ?, ?)"));
    for (int i = 0; i < childIds.size(); ++i) {
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, childIds[i]);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertActors(QSqlQuery& q, const QString& movieId, const QList<Person>& actors)
{
    if (actors.isEmpty()) return true;
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO movie_actors
            (movie_id, position, first_name, middle_name, last_name, birth_year,
             role, credited_as, voice, uncredited, puppeteer)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    for (int i = 0; i < actors.size(); ++i) {
        const auto& a = actors[i];
        q.bindValue(0,  movieId);
        q.bindValue(1,  i);
        q.bindValue(2,  a.firstName);
        q.bindValue(3,  a.middleName);
        q.bindValue(4,  a.lastName);
        q.bindValue(5,  a.birthYear);
        q.bindValue(6,  a.role);
        q.bindValue(7,  a.creditedAs);
        q.bindValue(8,  a.voice      ? 1 : 0);
        q.bindValue(9,  a.uncredited ? 1 : 0);
        q.bindValue(10, a.puppeteer  ? 1 : 0);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertCredits(QSqlQuery& q, const QString& movieId, const QList<Person>& credits)
{
    if (credits.isEmpty()) return true;
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO movie_credits
            (movie_id, position, first_name, middle_name, last_name, birth_year,
             credit_type, role, credited_as)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    for (int i = 0; i < credits.size(); ++i) {
        const auto& c = credits[i];
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, c.firstName);
        q.bindValue(3, c.middleName);
        q.bindValue(4, c.lastName);
        q.bindValue(5, c.birthYear);
        q.bindValue(6, c.creditType);
        q.bindValue(7, c.role);
        q.bindValue(8, c.creditedAs);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertAudioTracks(QSqlQuery& q, const QString& movieId, const QList<AudioTrack>& tracks)
{
    if (tracks.isEmpty()) return true;
    q.prepare(QStringLiteral(
        "INSERT INTO movie_audio_tracks (movie_id, position, content, format, channels) "
        "VALUES (?, ?, ?, ?, ?)"));
    for (int i = 0; i < tracks.size(); ++i) {
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, tracks[i].content);
        q.bindValue(3, tracks[i].format);
        q.bindValue(4, tracks[i].channels);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertDiscs(QSqlQuery& q, const QString& movieId, const QList<Disc>& discs)
{
    if (discs.isEmpty()) return true;
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO movie_discs
            (movie_id, position, description_side_a, description_side_b,
             disc_id_side_a, disc_id_side_b, label_side_a, label_side_b,
             dual_layered_side_a, dual_layered_side_b, dual_sided, location, slot)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    for (int i = 0; i < discs.size(); ++i) {
        const auto& d = discs[i];
        q.bindValue(0,  movieId);
        q.bindValue(1,  i);
        q.bindValue(2,  d.descriptionSideA);
        q.bindValue(3,  d.descriptionSideB);
        q.bindValue(4,  d.discIdSideA);
        q.bindValue(5,  d.discIdSideB);
        q.bindValue(6,  d.labelSideA);
        q.bindValue(7,  d.labelSideB);
        q.bindValue(8,  d.dualLayeredSideA ? 1 : 0);
        q.bindValue(9,  d.dualLayeredSideB ? 1 : 0);
        q.bindValue(10, d.dualSided ? 1 : 0);
        q.bindValue(11, d.location);
        q.bindValue(12, d.slot);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertCustomFields(QSqlQuery& q, const QString& movieId, const QList<CustomField>& cfs)
{
    if (cfs.isEmpty()) return true;
    q.prepare(QStringLiteral(
        "INSERT INTO movie_custom_fields (movie_id, position, name, value) "
        "VALUES (?, ?, ?, ?)"));
    for (int i = 0; i < cfs.size(); ++i) {
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, cfs[i].name);
        q.bindValue(3, cfs[i].value);
        if (!q.exec()) return false;
    }
    return true;
}

bool insertEvents(QSqlQuery& q, const QString& movieId, const QList<Event>& events)
{
    if (events.isEmpty()) return true;
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO movie_events
            (movie_id, position, event_type, timestamp, note,
             user_first_name, user_last_name, user_email, user_phone)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    for (int i = 0; i < events.size(); ++i) {
        const auto& e = events[i];
        q.bindValue(0, movieId);
        q.bindValue(1, i);
        q.bindValue(2, e.type);
        q.bindValue(3, isoOrEmpty(e.timestamp));
        q.bindValue(4, e.note);
        q.bindValue(5, e.userFirstName);
        q.bindValue(6, e.userLastName);
        q.bindValue(7, e.userEmail);
        q.bindValue(8, e.userPhone);
        if (!q.exec()) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

QStringList fetchSimpleList(QSqlDatabase& conn, const QString& table,
                            const QString& movieId)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(
        "SELECT value FROM %1 WHERE movie_id = ? ORDER BY position").arg(table));
    q.addBindValue(movieId);
    QStringList out;
    if (q.exec()) {
        while (q.next()) out << q.value(0).toString();
    }
    return out;
}

void loadActors(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        SELECT first_name, middle_name, last_name, birth_year, role, credited_as,
               voice, uncredited, puppeteer
        FROM movie_actors WHERE movie_id = ? ORDER BY position
    )sql"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        Person p;
        p.firstName  = q.value(0).toString();
        p.middleName = q.value(1).toString();
        p.lastName   = q.value(2).toString();
        p.birthYear  = q.value(3).toInt();
        p.role       = q.value(4).toString();
        p.creditedAs = q.value(5).toString();
        p.voice      = q.value(6).toBool();
        p.uncredited = q.value(7).toBool();
        p.puppeteer  = q.value(8).toBool();
        m.actors << p;
    }
}

void loadCredits(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        SELECT first_name, middle_name, last_name, birth_year,
               credit_type, role, credited_as
        FROM movie_credits WHERE movie_id = ? ORDER BY position
    )sql"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        Person p;
        p.firstName  = q.value(0).toString();
        p.middleName = q.value(1).toString();
        p.lastName   = q.value(2).toString();
        p.birthYear  = q.value(3).toInt();
        p.creditType = q.value(4).toString();
        p.role       = q.value(5).toString();
        p.creditedAs = q.value(6).toString();
        m.credits << p;
    }
}

void loadAudioTracks(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(
        "SELECT content, format, channels FROM movie_audio_tracks "
        "WHERE movie_id = ? ORDER BY position"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        AudioTrack t;
        t.content  = q.value(0).toString();
        t.format   = q.value(1).toString();
        t.channels = q.value(2).toString();
        m.audioTracks << t;
    }
}

void loadDiscs(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        SELECT description_side_a, description_side_b,
               disc_id_side_a, disc_id_side_b,
               label_side_a, label_side_b,
               dual_layered_side_a, dual_layered_side_b, dual_sided,
               location, slot
        FROM movie_discs WHERE movie_id = ? ORDER BY position
    )sql"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        Disc d;
        d.descriptionSideA = q.value(0).toString();
        d.descriptionSideB = q.value(1).toString();
        d.discIdSideA      = q.value(2).toString();
        d.discIdSideB      = q.value(3).toString();
        d.labelSideA       = q.value(4).toString();
        d.labelSideB       = q.value(5).toString();
        d.dualLayeredSideA = q.value(6).toBool();
        d.dualLayeredSideB = q.value(7).toBool();
        d.dualSided        = q.value(8).toBool();
        d.location         = q.value(9).toString();
        d.slot             = q.value(10).toString();
        m.discs << d;
    }
}

void loadCustomFields(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(
        "SELECT name, value FROM movie_custom_fields "
        "WHERE movie_id = ? ORDER BY position"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        CustomField c;
        c.name  = q.value(0).toString();
        c.value = q.value(1).toString();
        m.customFields << c;
    }
}

void loadEvents(QSqlDatabase& conn, Movie& m)
{
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        SELECT event_type, timestamp, note,
               user_first_name, user_last_name, user_email, user_phone
        FROM movie_events WHERE movie_id = ? ORDER BY position
    )sql"));
    q.addBindValue(m.id);
    if (!q.exec()) return;
    while (q.next()) {
        Event e;
        e.type          = q.value(0).toString();
        e.timestamp     = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
        e.note          = q.value(2).toString();
        e.userFirstName = q.value(3).toString();
        e.userLastName  = q.value(4).toString();
        e.userEmail     = q.value(5).toString();
        e.userPhone     = q.value(6).toString();
        m.events << e;
    }
}

// Column order must match the SELECT in the queries below.
const char* kMovieSelectColumns =
    "id, title, original_title, sort_title, upc, "
    "id_base, id_variant_num, id_locality_id, id_locality_description, id_type, "
    "dist_trait, production_year, release_date, running_time_minutes, format, "
    "overview, notes, easter_eggs, "
    "case_type, case_slip_cover, collection_type, is_part_of_owned, "
    "collection_number, count_as, wish_priority, my_links, other_features, "
    "location_id, cover_front_path, cover_back_path, "
    "purchase_price_value, purchase_price_denom_type, purchase_price_denom_desc, "
    "purchase_price_formatted, purchase_place, purchase_place_type, "
    "purchase_place_website, purchase_date, received_as_gift, "
    "gift_from_first_name, gift_from_last_name, "
    "srp_value, srp_denom_type, srp_denom_desc, srp_formatted, "
    "rating_system, rating_value, rating_age, rating_variant, rating_details, "
    "aspect_ratio, video_standard, color_mode, dimensions, "
    "letter_box, pan_and_scan, full_frame, enhanced_for_16x9, dual_sided, dual_layered, "
    "box_set_parent_id, box_set_is_parent, "
    "loaned, loan_due, loan_user_first_name, loan_user_last_name, "
    "loan_user_email, loan_user_phone, "
    "review_film, review_video, review_audio, review_extras, "
    "banner_front, banner_back, "
    "profile_timestamp, last_edited, "
    "tmdb_id";

Movie movieFromRow(QSqlQuery& q)
{
    Movie m;
    int i = 0;
    m.id                              = q.value(i++).toString();
    m.title                           = q.value(i++).toString();
    m.originalTitle                   = q.value(i++).toString();
    m.sortTitle                       = q.value(i++).toString();
    m.upc                             = q.value(i++).toString();
    m.idMetadata.base                 = q.value(i++).toString();
    m.idMetadata.variantNum           = q.value(i++).toInt();
    m.idMetadata.localityId           = q.value(i++).toInt();
    m.idMetadata.localityDescription  = q.value(i++).toString();
    m.idMetadata.type                 = q.value(i++).toString();
    m.distTrait                       = q.value(i++).toString();
    m.productionYear                  = q.value(i++).toInt();
    m.releaseDate                     = QDate::fromString(q.value(i++).toString(), Qt::ISODate);
    m.runningTimeMinutes              = q.value(i++).toInt();
    m.format                          = q.value(i++).toString();
    m.overview                        = q.value(i++).toString();
    m.notes                           = q.value(i++).toString();
    m.easterEggs                      = q.value(i++).toString();
    m.caseType                        = q.value(i++).toString();
    m.caseSlipCover                   = q.value(i++).toBool();
    m.membership.type                 = q.value(i++).toString();
    m.membership.isPartOfOwnedCollection = q.value(i++).toBool();
    m.collectionNumber                = q.value(i++).toInt();
    m.countAs                         = q.value(i++).toInt();
    m.wishPriority                    = q.value(i++).toInt();
    m.myLinks                         = q.value(i++).toString();
    m.otherFeatures                   = q.value(i++).toString();
    m.locationId                      = q.value(i++).toString();
    m.coverFrontPath                  = q.value(i++).toString();
    m.coverBackPath                   = q.value(i++).toString();
    m.purchase.price.value                   = q.value(i++).toString();
    m.purchase.price.denominationType        = q.value(i++).toString();
    m.purchase.price.denominationDescription = q.value(i++).toString();
    m.purchase.price.formattedValue          = q.value(i++).toString();
    m.purchase.place                  = q.value(i++).toString();
    m.purchase.placeType              = q.value(i++).toString();
    m.purchase.placeWebsite           = q.value(i++).toString();
    m.purchase.date                   = QDate::fromString(q.value(i++).toString(), Qt::ISODate);
    m.purchase.receivedAsGift         = q.value(i++).toBool();
    m.purchase.giftFromFirstName      = q.value(i++).toString();
    m.purchase.giftFromLastName       = q.value(i++).toString();
    m.srp.value                       = q.value(i++).toString();
    m.srp.denominationType            = q.value(i++).toString();
    m.srp.denominationDescription     = q.value(i++).toString();
    m.srp.formattedValue              = q.value(i++).toString();
    m.rating.system                   = q.value(i++).toString();
    m.rating.value                    = q.value(i++).toString();
    m.rating.age                      = q.value(i++).toInt();
    m.rating.variant                  = q.value(i++).toInt();
    m.rating.details                  = q.value(i++).toString();
    m.videoFormat.aspectRatio         = q.value(i++).toString();
    m.videoFormat.videoStandard       = q.value(i++).toString();
    m.videoFormat.colorMode           = q.value(i++).toString();
    m.videoFormat.dimensions          = q.value(i++).toString();
    m.videoFormat.letterBox           = q.value(i++).toBool();
    m.videoFormat.panAndScan          = q.value(i++).toBool();
    m.videoFormat.fullFrame           = q.value(i++).toBool();
    m.videoFormat.enhancedFor16x9     = q.value(i++).toBool();
    m.videoFormat.dualSided           = q.value(i++).toBool();
    m.videoFormat.dualLayered         = q.value(i++).toBool();
    m.boxSet.parentId                 = q.value(i++).toString();
    m.boxSet.isParent                 = q.value(i++).toBool();
    m.loan.loaned                     = q.value(i++).toBool();
    m.loan.due                        = QDate::fromString(q.value(i++).toString(), Qt::ISODate);
    m.loan.userFirstName              = q.value(i++).toString();
    m.loan.userLastName               = q.value(i++).toString();
    m.loan.userEmail                  = q.value(i++).toString();
    m.loan.userPhone                  = q.value(i++).toString();
    m.review.film                     = q.value(i++).toInt();
    m.review.video                    = q.value(i++).toInt();
    m.review.audio                    = q.value(i++).toInt();
    m.review.extras                   = q.value(i++).toInt();
    m.mediaBanners.front              = q.value(i++).toString();
    m.mediaBanners.back               = q.value(i++).toString();
    m.profileTimestamp                = QDateTime::fromString(q.value(i++).toString(), Qt::ISODate);
    m.lastEdited                      = QDateTime::fromString(q.value(i++).toString(), Qt::ISODate);
    m.tmdbId                          = q.value(i++).toInt();
    return m;
}

} // namespace

// ---------------------------------------------------------------------------
// MovieRepository
// ---------------------------------------------------------------------------

MovieRepository::MovieRepository(Database& db) : m_db(db) {}

QString MovieRepository::toRelativeCover_(const QString& path) const
{
    if (path.isEmpty() || m_libraryRoot.isEmpty()) return path;
    const QDir root(m_libraryRoot);
    const QString rel = root.relativeFilePath(path);
    return rel;
}

QString MovieRepository::fromRelativeCover_(const QString& path) const
{
    if (path.isEmpty() || m_libraryRoot.isEmpty()) return path;
    if (QDir::isAbsolutePath(path)) return path;
    return QDir(m_libraryRoot).filePath(path);
}

bool MovieRepository::insert(const Movie& movie)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    if (!deleteChildren(q, movie.id)) {
        m_lastError = q.lastError().text();
        return false;
    }
    return insertRow_(movie)
        && insertChildren_(movie)
        && insertFts_(movie);
}

bool MovieRepository::bulkInsert(const QList<Movie>& movies)
{
    auto conn = m_db.handle();
    if (!conn.transaction()) {
        m_lastError = QStringLiteral("BEGIN failed: %1").arg(conn.lastError().text());
        return false;
    }
    for (const auto& movie : movies) {
        if (!insert(movie)) {
            conn.rollback();
            return false;
        }
    }
    if (!conn.commit()) {
        m_lastError = QStringLiteral("COMMIT failed: %1").arg(conn.lastError().text());
        return false;
    }
    return true;
}

int MovieRepository::count()
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM movies"))) {
        m_lastError = q.lastError().text();
        return -1;
    }
    if (q.next()) return q.value(0).toInt();
    return 0;
}

std::optional<Movie> MovieRepository::getById(const QString& id)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    q.prepare(QStringLiteral("SELECT %1 FROM movies WHERE id = ?")
                  .arg(QLatin1String(kMovieSelectColumns)));
    q.addBindValue(id);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return std::nullopt;
    }
    if (!q.next()) return std::nullopt;

    Movie m = movieFromRow(q);
    loadChildren_(m);

    m.coverFrontPath = fromRelativeCover_(m.coverFrontPath);
    m.coverBackPath  = fromRelativeCover_(m.coverBackPath);
    return m;
}

QList<Movie> MovieRepository::getAll()
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    if (!q.exec(QStringLiteral("SELECT %1 FROM movies ORDER BY sort_title, title")
                    .arg(QLatin1String(kMovieSelectColumns)))) {
        m_lastError = q.lastError().text();
        return {};
    }
    QList<Movie> out;
    while (q.next()) {
        out << movieFromRow(q);
    }
    for (auto& m : out) {
        loadChildren_(m);
        m.coverFrontPath = fromRelativeCover_(m.coverFrontPath);
        m.coverBackPath  = fromRelativeCover_(m.coverBackPath);
    }
    return out;
}

QList<Movie> MovieRepository::search(const QString& query, int limit)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        SELECT %1 FROM movies
        WHERE id IN (
            SELECT movie_id FROM movies_fts
            WHERE movies_fts MATCH ?
            ORDER BY bm25(movies_fts)
            LIMIT ?
        )
    )sql").arg(QLatin1String(kMovieSelectColumns)));
    q.addBindValue(query);
    q.addBindValue(limit);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return {};
    }
    QList<Movie> out;
    while (q.next()) out << movieFromRow(q);
    for (auto& m : out) {
        loadChildren_(m);
        m.coverFrontPath = fromRelativeCover_(m.coverFrontPath);
        m.coverBackPath  = fromRelativeCover_(m.coverBackPath);
    }
    return out;
}

bool MovieRepository::insertRow_(const Movie& movie)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        INSERT OR REPLACE INTO movies (
            id, title, original_title, sort_title, upc,
            id_base, id_variant_num, id_locality_id, id_locality_description, id_type,
            dist_trait, production_year, release_date, running_time_minutes, format,
            overview, notes, easter_eggs,
            case_type, case_slip_cover, collection_type, is_part_of_owned,
            collection_number, count_as, wish_priority, my_links, other_features,
            location_id, cover_front_path, cover_back_path,
            purchase_price_value, purchase_price_denom_type, purchase_price_denom_desc,
            purchase_price_formatted, purchase_place, purchase_place_type,
            purchase_place_website, purchase_date, received_as_gift,
            gift_from_first_name, gift_from_last_name,
            srp_value, srp_denom_type, srp_denom_desc, srp_formatted,
            rating_system, rating_value, rating_age, rating_variant, rating_details,
            aspect_ratio, video_standard, color_mode, dimensions,
            letter_box, pan_and_scan, full_frame, enhanced_for_16x9, dual_sided, dual_layered,
            box_set_parent_id, box_set_is_parent,
            loaned, loan_due, loan_user_first_name, loan_user_last_name,
            loan_user_email, loan_user_phone,
            review_film, review_video, review_audio, review_extras,
            banner_front, banner_back,
            profile_timestamp, last_edited,
            tmdb_id
        ) VALUES (
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?, ?,
            ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?, ?, ?,
            ?, ?, ?, ?,
            ?, ?, ?, ?, ?, ?,
            ?, ?,
            ?, ?, ?, ?,
            ?, ?,
            ?, ?, ?, ?,
            ?, ?,
            ?, ?,
            ?
        )
    )sql"));

    int i = 0;
    q.bindValue(i++, movie.id);
    q.bindValue(i++, movie.title);
    q.bindValue(i++, movie.originalTitle);
    q.bindValue(i++, movie.sortTitle);
    q.bindValue(i++, movie.upc);
    q.bindValue(i++, movie.idMetadata.base);
    q.bindValue(i++, movie.idMetadata.variantNum);
    q.bindValue(i++, movie.idMetadata.localityId);
    q.bindValue(i++, movie.idMetadata.localityDescription);
    q.bindValue(i++, movie.idMetadata.type);
    q.bindValue(i++, movie.distTrait);
    q.bindValue(i++, movie.productionYear);
    q.bindValue(i++, isoOrEmpty(movie.releaseDate));
    q.bindValue(i++, movie.runningTimeMinutes);
    q.bindValue(i++, movie.format);
    q.bindValue(i++, movie.overview);
    q.bindValue(i++, movie.notes);
    q.bindValue(i++, movie.easterEggs);
    q.bindValue(i++, movie.caseType);
    q.bindValue(i++, movie.caseSlipCover ? 1 : 0);
    q.bindValue(i++, movie.membership.type);
    q.bindValue(i++, movie.membership.isPartOfOwnedCollection ? 1 : 0);
    q.bindValue(i++, movie.collectionNumber);
    q.bindValue(i++, movie.countAs);
    q.bindValue(i++, movie.wishPriority);
    q.bindValue(i++, movie.myLinks);
    q.bindValue(i++, movie.otherFeatures);
    q.bindValue(i++, movie.locationId);
    q.bindValue(i++, toRelativeCover_(movie.coverFrontPath));
    q.bindValue(i++, toRelativeCover_(movie.coverBackPath));
    q.bindValue(i++, movie.purchase.price.value);
    q.bindValue(i++, movie.purchase.price.denominationType);
    q.bindValue(i++, movie.purchase.price.denominationDescription);
    q.bindValue(i++, movie.purchase.price.formattedValue);
    q.bindValue(i++, movie.purchase.place);
    q.bindValue(i++, movie.purchase.placeType);
    q.bindValue(i++, movie.purchase.placeWebsite);
    q.bindValue(i++, isoOrEmpty(movie.purchase.date));
    q.bindValue(i++, movie.purchase.receivedAsGift ? 1 : 0);
    q.bindValue(i++, movie.purchase.giftFromFirstName);
    q.bindValue(i++, movie.purchase.giftFromLastName);
    q.bindValue(i++, movie.srp.value);
    q.bindValue(i++, movie.srp.denominationType);
    q.bindValue(i++, movie.srp.denominationDescription);
    q.bindValue(i++, movie.srp.formattedValue);
    q.bindValue(i++, movie.rating.system);
    q.bindValue(i++, movie.rating.value);
    q.bindValue(i++, movie.rating.age);
    q.bindValue(i++, movie.rating.variant);
    q.bindValue(i++, movie.rating.details);
    q.bindValue(i++, movie.videoFormat.aspectRatio);
    q.bindValue(i++, movie.videoFormat.videoStandard);
    q.bindValue(i++, movie.videoFormat.colorMode);
    q.bindValue(i++, movie.videoFormat.dimensions);
    q.bindValue(i++, movie.videoFormat.letterBox       ? 1 : 0);
    q.bindValue(i++, movie.videoFormat.panAndScan      ? 1 : 0);
    q.bindValue(i++, movie.videoFormat.fullFrame       ? 1 : 0);
    q.bindValue(i++, movie.videoFormat.enhancedFor16x9 ? 1 : 0);
    q.bindValue(i++, movie.videoFormat.dualSided       ? 1 : 0);
    q.bindValue(i++, movie.videoFormat.dualLayered     ? 1 : 0);
    q.bindValue(i++, movie.boxSet.parentId);
    q.bindValue(i++, movie.boxSet.isParent ? 1 : 0);
    q.bindValue(i++, movie.loan.loaned ? 1 : 0);
    q.bindValue(i++, isoOrEmpty(movie.loan.due));
    q.bindValue(i++, movie.loan.userFirstName);
    q.bindValue(i++, movie.loan.userLastName);
    q.bindValue(i++, movie.loan.userEmail);
    q.bindValue(i++, movie.loan.userPhone);
    q.bindValue(i++, movie.review.film);
    q.bindValue(i++, movie.review.video);
    q.bindValue(i++, movie.review.audio);
    q.bindValue(i++, movie.review.extras);
    q.bindValue(i++, movie.mediaBanners.front);
    q.bindValue(i++, movie.mediaBanners.back);
    q.bindValue(i++, isoOrEmpty(movie.profileTimestamp));
    q.bindValue(i++, isoOrEmpty(movie.lastEdited));
    q.bindValue(i++, movie.tmdbId > 0 ? QVariant(movie.tmdbId) : QVariant(QMetaType(QMetaType::Int)));

    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool MovieRepository::insertChildren_(const Movie& movie)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    const QString& id = movie.id;

    const bool ok =
        insertSimpleList     (q, QStringLiteral("movie_genres"),          id, movie.genres) &&
        insertSimpleList     (q, QStringLiteral("movie_tags"),            id, movie.tags) &&
        insertSimpleList     (q, QStringLiteral("movie_studios"),         id, movie.studios) &&
        insertSimpleList     (q, QStringLiteral("movie_media_companies"), id, movie.mediaCompanies) &&
        insertSimpleList     (q, QStringLiteral("movie_subtitles"),       id, movie.subtitles) &&
        insertSimpleList     (q, QStringLiteral("movie_countries"),       id, movie.countriesOfOrigin) &&
        insertSimpleList     (q, QStringLiteral("movie_regions"),         id, movie.regions) &&
        insertSimpleList     (q, QStringLiteral("movie_features"),        id, movie.features) &&
        insertSimpleList     (q, QStringLiteral("movie_locked_fields"),   id, movie.lockedFields) &&
        insertBoxSetChildren (q, id, movie.boxSet.childIds) &&
        insertActors         (q, id, movie.actors) &&
        insertCredits        (q, id, movie.credits) &&
        insertAudioTracks    (q, id, movie.audioTracks) &&
        insertDiscs          (q, id, movie.discs) &&
        insertCustomFields   (q, id, movie.customFields) &&
        insertEvents         (q, id, movie.events);

    if (!ok) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool MovieRepository::insertFts_(const Movie& movie)
{
    auto conn = m_db.handle();
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(R"sql(
        INSERT INTO movies_fts (
            movie_id, title, original_title, overview, notes, easter_eggs,
            actors, credits, studios
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )sql"));
    q.addBindValue(movie.id);
    q.addBindValue(movie.title);
    q.addBindValue(movie.originalTitle);
    q.addBindValue(movie.overview);
    q.addBindValue(movie.notes);
    q.addBindValue(movie.easterEggs);
    q.addBindValue(joinPeople(movie.actors));
    q.addBindValue(joinPeople(movie.credits));
    q.addBindValue(movie.studios.join(QChar(u'\n')));
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

void MovieRepository::loadChildren_(Movie& m)
{
    auto conn = m_db.handle();
    m.genres            = fetchSimpleList(conn, QStringLiteral("movie_genres"),          m.id);
    m.tags              = fetchSimpleList(conn, QStringLiteral("movie_tags"),            m.id);
    m.studios           = fetchSimpleList(conn, QStringLiteral("movie_studios"),         m.id);
    m.mediaCompanies    = fetchSimpleList(conn, QStringLiteral("movie_media_companies"), m.id);
    m.subtitles         = fetchSimpleList(conn, QStringLiteral("movie_subtitles"),       m.id);
    m.countriesOfOrigin = fetchSimpleList(conn, QStringLiteral("movie_countries"),       m.id);
    m.regions           = fetchSimpleList(conn, QStringLiteral("movie_regions"),         m.id);
    m.features          = fetchSimpleList(conn, QStringLiteral("movie_features"),        m.id);
    m.lockedFields      = fetchSimpleList(conn, QStringLiteral("movie_locked_fields"),   m.id);

    // box-set children use a different column name
    QSqlQuery q(conn);
    q.prepare(QStringLiteral(
        "SELECT child_id FROM movie_box_set_children "
        "WHERE movie_id = ? ORDER BY position"));
    q.addBindValue(m.id);
    if (q.exec()) {
        while (q.next()) m.boxSet.childIds << q.value(0).toString();
    }

    loadActors      (conn, m);
    loadCredits     (conn, m);
    loadAudioTracks (conn, m);
    loadDiscs       (conn, m);
    loadCustomFields(conn, m);
    loadEvents      (conn, m);
}

} // namespace xyz
