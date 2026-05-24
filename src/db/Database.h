#pragma once

#include <QSqlDatabase>
#include <QString>

namespace xyz {

// Owns a single SQLite connection. The connection name is unique per
// instance (a UUID) so multiple `Database` objects in the same process —
// production library, in-memory test instances — never collide on
// `QSqlDatabase::database()` lookups.
//
// On open(), pragmas are set that we always want:
//   - foreign_keys = ON  (FK constraints enforced)
//   - journal_mode = WAL (better write concurrency, durable)
//   - synchronous  = NORMAL (good default for desktop apps)
//
// Use ":memory:" as the path for ephemeral databases (useful in tests).
class Database {
public:
    Database();
    ~Database();

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    // Open the SQLite file at `path`. Returns true on success; if false,
    // `errorString()` carries the diagnostic.
    bool    open(const QString& path);
    void    close();

    bool    isOpen() const;
    QString path() const { return m_path; }
    QString errorString() const { return m_errorString; }

    // Returns the underlying QSqlDatabase. Callers should not store a
    // copy across the Database's lifetime — when this object goes away
    // the connection is removed.
    QSqlDatabase handle() const;

private:
    QString m_connectionName;
    QString m_path;
    QString m_errorString;
};

} // namespace xyz
