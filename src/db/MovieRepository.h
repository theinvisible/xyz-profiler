#pragma once

#include "domain/Movie.h"

#include <QList>
#include <QString>
#include <optional>

namespace xyz {

class Database;

// CRUD + bulk-import + FTS5 search for the `movies` table family.
//
// The repository owns no state beyond a reference to the `Database` it
// reads/writes through. `bulkInsert()` wraps its work in a single
// transaction; one-shot `insert()` writes immediately. Reads reconstruct
// the full `Movie` object, including all child collections.
//
// Cover paths are stored **relative to `libraryRoot()`**. If the library
// root is configured, absolute paths handed to the repository are
// shortened on write and re-prefixed on read, so moving the library
// directory only requires updating the root setting.
class MovieRepository {
public:
    explicit MovieRepository(Database& db);

    void    setLibraryRoot(const QString& root) { m_libraryRoot = root; }
    QString libraryRoot() const { return m_libraryRoot; }

    QString lastError() const { return m_lastError; }

    // Insert (or replace by id) a single movie. Returns true on success.
    bool insert(const Movie& movie);

    // Delete a movie and all its child rows by primary id. Child tables drop
    // via ON DELETE CASCADE; the FTS5 row is removed explicitly (virtual tables
    // don't cascade). Returns true on success (including when no row matched).
    bool remove(const QString& id);

    // Update only the front-cover path for one movie (single UPDATE, no child
    // re-insert). Much cheaper than insert() when only the poster changed —
    // used after a poster download. `absolutePath` is relativized to the
    // library root like any other cover path.
    bool setCoverFront(const QString& id, const QString& absolutePath);
    bool setCoverBack(const QString& id, const QString& absolutePath);

    // Insert many movies inside a single transaction. Faster for imports.
    // Returns true on success; on failure the transaction is rolled back
    // and the database is unchanged.
    bool bulkInsert(const QList<Movie>& movies);

    // Number of rows in the movies table.
    int count();

    // Fetch a single movie by primary id. Returns nullopt if not found.
    std::optional<Movie> getById(const QString& id);

    // Fetch every movie. Heavy on a large library; prefer `search()` or
    // paginated queries for UI work.
    QList<Movie> getAll();

    // Full-text search against the FTS5 index. Hits are returned ranked
    // (most relevant first) by SQLite's built-in bm25 scoring.
    // `query` accepts FTS5 query syntax (e.g. `tom hanks`, `"forrest gump"`,
    // `actors:hanks`).
    QList<Movie> search(const QString& query, int limit = 100);

private:
    bool insertRow_(const Movie& movie);
    bool insertChildren_(const Movie& movie);
    bool insertFts_(const Movie& movie);
    void loadChildren_(Movie& movie);

    QString toRelativeCover_(const QString& path) const;
    QString fromRelativeCover_(const QString& path) const;

    Database& m_db;
    QString   m_libraryRoot;
    QString   m_lastError;
};

} // namespace xyz
