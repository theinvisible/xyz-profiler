#include "Migrations.h"

#include "Database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace xyz {
namespace {

// ---------------------------------------------------------------------------
// Migration v1 — initial schema
//
// One main `movies` table that mirrors the flat fields of the `Movie`
// struct, plus child tables for the list- and struct-valued members. All
// child tables key on (`movie_id`, `position`) so ordering round-trips.
//
// FTS5 virtual table `movies_fts` aggregates the searchable text (title,
// overview, notes, easter eggs, plus newline-joined people and studios).
// `MovieRepository::insert()` populates it from the same Movie struct in
// the same transaction — no triggers, keeps the schema simple and
// re-population predictable.
// ---------------------------------------------------------------------------
const QStringList kV1Statements = {
    QStringLiteral(R"sql(
        CREATE TABLE movies (
            id                              TEXT PRIMARY KEY,
            title                           TEXT NOT NULL,
            original_title                  TEXT,
            sort_title                      TEXT,
            upc                             TEXT,
            id_base                         TEXT,
            id_variant_num                  INTEGER NOT NULL DEFAULT 0,
            id_locality_id                  INTEGER NOT NULL DEFAULT 0,
            id_locality_description         TEXT,
            id_type                         TEXT,
            dist_trait                      TEXT,
            production_year                 INTEGER NOT NULL DEFAULT 0,
            release_date                    TEXT,
            running_time_minutes            INTEGER NOT NULL DEFAULT 0,
            format                          TEXT,
            overview                        TEXT,
            notes                           TEXT,
            easter_eggs                     TEXT,
            case_type                       TEXT,
            case_slip_cover                 INTEGER NOT NULL DEFAULT 0,
            collection_type                 TEXT,
            is_part_of_owned                INTEGER NOT NULL DEFAULT 1,
            collection_number               INTEGER NOT NULL DEFAULT 0,
            count_as                        INTEGER NOT NULL DEFAULT 1,
            wish_priority                   INTEGER NOT NULL DEFAULT 0,
            my_links                        TEXT,
            other_features                  TEXT,
            location_id                     TEXT,
            cover_front_path                TEXT,
            cover_back_path                 TEXT,

            purchase_price_value            TEXT,
            purchase_price_denom_type       TEXT,
            purchase_price_denom_desc       TEXT,
            purchase_price_formatted        TEXT,
            purchase_place                  TEXT,
            purchase_place_type             TEXT,
            purchase_place_website          TEXT,
            purchase_date                   TEXT,
            received_as_gift                INTEGER NOT NULL DEFAULT 0,
            gift_from_first_name            TEXT,
            gift_from_last_name             TEXT,

            srp_value                       TEXT,
            srp_denom_type                  TEXT,
            srp_denom_desc                  TEXT,
            srp_formatted                   TEXT,

            rating_system                   TEXT,
            rating_value                    TEXT,
            rating_age                      INTEGER NOT NULL DEFAULT 0,
            rating_variant                  INTEGER NOT NULL DEFAULT 0,
            rating_details                  TEXT,

            aspect_ratio                    TEXT,
            video_standard                  TEXT,
            color_mode                      TEXT,
            dimensions                      TEXT,
            letter_box                      INTEGER NOT NULL DEFAULT 0,
            pan_and_scan                    INTEGER NOT NULL DEFAULT 0,
            full_frame                      INTEGER NOT NULL DEFAULT 0,
            enhanced_for_16x9               INTEGER NOT NULL DEFAULT 0,
            dual_sided                      INTEGER NOT NULL DEFAULT 0,
            dual_layered                    INTEGER NOT NULL DEFAULT 0,

            box_set_parent_id               TEXT,
            box_set_is_parent               INTEGER NOT NULL DEFAULT 0,

            loaned                          INTEGER NOT NULL DEFAULT 0,
            loan_due                        TEXT,
            loan_user_first_name            TEXT,
            loan_user_last_name             TEXT,
            loan_user_email                 TEXT,
            loan_user_phone                 TEXT,

            review_film                     INTEGER NOT NULL DEFAULT 0,
            review_video                    INTEGER NOT NULL DEFAULT 0,
            review_audio                    INTEGER NOT NULL DEFAULT 0,
            review_extras                   INTEGER NOT NULL DEFAULT 0,

            banner_front                    TEXT,
            banner_back                     TEXT,

            profile_timestamp               TEXT,
            last_edited                     TEXT
        )
    )sql"),

    QStringLiteral("CREATE INDEX idx_movies_title       ON movies(title)"),
    QStringLiteral("CREATE INDEX idx_movies_sort_title  ON movies(sort_title)"),
    QStringLiteral("CREATE INDEX idx_movies_year        ON movies(production_year)"),
    QStringLiteral("CREATE INDEX idx_movies_box_parent  ON movies(box_set_parent_id)"),

    // ---- Simple list child tables -----------------------------------------
    QStringLiteral("CREATE TABLE movie_genres          (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_tags            (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_studios         (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_media_companies (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_subtitles       (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_countries       (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_regions         (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_features        (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_locked_fields   (movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, value TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),
    QStringLiteral("CREATE TABLE movie_box_set_children(movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE, position INTEGER NOT NULL, child_id TEXT NOT NULL, PRIMARY KEY (movie_id, position))"),

    // Look-up indexes for common "who is in what" queries.
    QStringLiteral("CREATE INDEX idx_genres_value   ON movie_genres(value)"),
    QStringLiteral("CREATE INDEX idx_studios_value  ON movie_studios(value)"),
    QStringLiteral("CREATE INDEX idx_features_value ON movie_features(value)"),

    // ---- Structured child tables -----------------------------------------
    QStringLiteral(R"sql(
        CREATE TABLE movie_actors (
            movie_id    TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position    INTEGER NOT NULL,
            first_name  TEXT,
            middle_name TEXT,
            last_name   TEXT,
            birth_year  INTEGER NOT NULL DEFAULT 0,
            role        TEXT,
            credited_as TEXT,
            voice       INTEGER NOT NULL DEFAULT 0,
            uncredited  INTEGER NOT NULL DEFAULT 0,
            puppeteer   INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),
    QStringLiteral("CREATE INDEX idx_actors_last_name ON movie_actors(last_name)"),

    QStringLiteral(R"sql(
        CREATE TABLE movie_credits (
            movie_id    TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position    INTEGER NOT NULL,
            first_name  TEXT,
            middle_name TEXT,
            last_name   TEXT,
            birth_year  INTEGER NOT NULL DEFAULT 0,
            credit_type TEXT,
            role        TEXT,
            credited_as TEXT,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),
    QStringLiteral("CREATE INDEX idx_credits_last_name ON movie_credits(last_name)"),
    QStringLiteral("CREATE INDEX idx_credits_type      ON movie_credits(credit_type)"),

    QStringLiteral(R"sql(
        CREATE TABLE movie_audio_tracks (
            movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            content  TEXT,
            format   TEXT,
            channels TEXT,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),

    QStringLiteral(R"sql(
        CREATE TABLE movie_discs (
            movie_id              TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position              INTEGER NOT NULL,
            description_side_a    TEXT,
            description_side_b    TEXT,
            disc_id_side_a        TEXT,
            disc_id_side_b        TEXT,
            label_side_a          TEXT,
            label_side_b          TEXT,
            dual_layered_side_a   INTEGER NOT NULL DEFAULT 0,
            dual_layered_side_b   INTEGER NOT NULL DEFAULT 0,
            dual_sided            INTEGER NOT NULL DEFAULT 0,
            location              TEXT,
            slot                  TEXT,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),

    QStringLiteral(R"sql(
        CREATE TABLE movie_custom_fields (
            movie_id TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position INTEGER NOT NULL,
            name     TEXT,
            value    TEXT,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),

    QStringLiteral(R"sql(
        CREATE TABLE movie_events (
            movie_id        TEXT NOT NULL REFERENCES movies(id) ON DELETE CASCADE,
            position        INTEGER NOT NULL,
            event_type      TEXT,
            timestamp       TEXT,
            note            TEXT,
            user_first_name TEXT,
            user_last_name  TEXT,
            user_email      TEXT,
            user_phone      TEXT,
            PRIMARY KEY (movie_id, position)
        )
    )sql"),

    // ---- FTS5 over searchable text ---------------------------------------
    // `movie_id` is unindexed (we only need it to join back to `movies`).
    // The remaining columns are indexed and ranked. The repository fills
    // them as a denormalised view in the same transaction as movies.
    QStringLiteral(R"sql(
        CREATE VIRTUAL TABLE movies_fts USING fts5(
            movie_id UNINDEXED,
            title,
            original_title,
            overview,
            notes,
            easter_eggs,
            actors,
            credits,
            studios,
            tokenize = 'unicode61 remove_diacritics 2'
        )
    )sql"),
};

bool exec(QSqlQuery& q, const QString& sql, QString* err)
{
    if (!q.exec(sql)) {
        if (err) *err = QStringLiteral("SQL failed: %1\n  -> %2")
                            .arg(q.lastError().text(), sql.left(120));
        return false;
    }
    return true;
}

bool ensureSchemaVersionTable(QSqlQuery& q, QString* err)
{
    if (!exec(q, QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)"), err))
        return false;
    if (!exec(q, QStringLiteral("SELECT COUNT(*) FROM schema_version"), err))
        return false;
    int rowCount = 0;
    if (q.next()) rowCount = q.value(0).toInt();
    if (rowCount == 0) {
        if (!exec(q, QStringLiteral("INSERT INTO schema_version (version) VALUES (0)"), err))
            return false;
    }
    return true;
}

bool applyMigration(QSqlQuery& q, int version, QString* err)
{
    switch (version) {
    case 1:
        for (const auto& sql : kV1Statements) {
            if (!exec(q, sql, err)) return false;
        }
        return true;
    default:
        if (err) *err = QStringLiteral("Unknown migration version %1").arg(version);
        return false;
    }
}

} // namespace

int Migrations::latestVersion()
{
    return 1;
}

int Migrations::currentVersion(Database& db, QString* errorString)
{
    auto conn = db.handle();
    QSqlQuery q(conn);

    if (!q.exec(QStringLiteral(
            "SELECT name FROM sqlite_master "
            "WHERE type='table' AND name='schema_version'"))) {
        if (errorString) *errorString = q.lastError().text();
        return -1;
    }
    if (!q.next()) return 0;

    if (!q.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
        if (errorString) *errorString = q.lastError().text();
        return -1;
    }
    if (q.next()) return q.value(0).toInt();
    return 0;
}

bool Migrations::migrateToLatest(Database& db, QString* errorString)
{
    auto conn = db.handle();
    QSqlQuery q(conn);

    QString localErr;
    if (!ensureSchemaVersionTable(q, &localErr)) {
        if (errorString) *errorString = localErr;
        return false;
    }

    if (!q.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) {
        if (errorString) *errorString = q.lastError().text();
        return false;
    }
    int current = 0;
    if (q.next()) current = q.value(0).toInt();

    for (int v = current + 1; v <= latestVersion(); ++v) {
        if (!conn.transaction()) {
            if (errorString) *errorString = QStringLiteral("BEGIN failed: %1")
                .arg(conn.lastError().text());
            return false;
        }
        if (!applyMigration(q, v, &localErr) ||
            !exec(q, QStringLiteral("UPDATE schema_version SET version = %1").arg(v), &localErr)) {
            conn.rollback();
            if (errorString) *errorString = QStringLiteral("Migration v%1 failed: %2")
                .arg(v).arg(localErr);
            return false;
        }
        if (!conn.commit()) {
            if (errorString) *errorString = QStringLiteral("COMMIT v%1 failed: %2")
                .arg(v).arg(conn.lastError().text());
            return false;
        }
    }
    return true;
}

} // namespace xyz
