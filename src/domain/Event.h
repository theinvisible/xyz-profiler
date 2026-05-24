#pragma once

#include <QDateTime>
#include <QString>

namespace xyz {

// Item-level history entry (e.g. "Borrowed", "Returned", "Watched").
// DP4 ships these inside <Events>/<Event> alongside the User element that
// carries the same shape as in LoanInfo.
struct Event {
    QString   type;          // "Borrowed", "Returned", ...
    QDateTime timestamp;     // ISO-8601 with ms precision in DP4
    QString   note;
    QString   userFirstName;
    QString   userLastName;
    QString   userEmail;
    QString   userPhone;
};

} // namespace xyz
