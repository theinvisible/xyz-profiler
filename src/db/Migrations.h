#pragma once

#include <QString>

namespace xyz {

class Database;

// Versioned schema migrations.
//
// Schema version is stored in a single-row table `schema_version(version)`.
// Calling `migrateToLatest()` is idempotent: each migration runs once, in
// version order, inside a transaction. Subsequent calls with no pending
// migrations are no-ops.
//
// To add a new migration: bump `latestVersion()` and append a case to
// `applyMigration()`. Never edit an already-released migration.
class Migrations {
public:
    // The newest version this binary knows how to migrate to.
    static int latestVersion();

    // Bring `db`'s schema up to `latestVersion()`. Returns true on success;
    // on failure, `errorString` holds the diagnostic.
    static bool migrateToLatest(Database& db, QString* errorString = nullptr);

    // Read the current schema version. Returns 0 for a brand-new database.
    static int currentVersion(Database& db, QString* errorString = nullptr);
};

} // namespace xyz
