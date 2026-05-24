#include "Database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace xyz {

Database::Database()
    : m_connectionName(QStringLiteral("xyz_db_") +
                       QUuid::createUuid().toString(QUuid::WithoutBraces))
{}

Database::~Database()
{
    close();
}

bool Database::open(const QString& path)
{
    if (isOpen()) close();

    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                        m_connectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        m_errorString = db.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }

    QSqlQuery q(db);
    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous  = NORMAL"),
    };
    for (const auto& sql : pragmas) {
        if (!q.exec(sql)) {
            m_errorString = QStringLiteral("Pragma failed (%1): %2")
                                .arg(sql, q.lastError().text());
            db.close();
            QSqlDatabase::removeDatabase(m_connectionName);
            return false;
        }
    }

    m_path = path;
    m_errorString.clear();
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            auto db = QSqlDatabase::database(m_connectionName);
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_path.clear();
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) &&
           QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

} // namespace xyz
