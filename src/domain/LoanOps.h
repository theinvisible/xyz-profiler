#pragma once

#include "domain/Event.h"
#include "domain/Movie.h"

#include <QDate>
#include <QDateTime>
#include <QString>

namespace xyz {

// Lending mutations, kept pure so they can be unit-tested without a GUI or a
// database (same split as CalendarBuckets). The controller applies them to a
// Movie copy and hands the result to LibraryController::updateMovie.
//
// `when` is a parameter rather than QDateTime::currentDateTime() so tests are
// deterministic.
//
// The event type strings match the vocabulary DP4 writes into <Events>/<Event>
// (see domain/Event.h), so imported history and locally created history read
// as one list.

inline QString loanEventBorrowed() { return QStringLiteral("Borrowed"); }
inline QString loanEventReturned() { return QStringLiteral("Returned"); }

inline void lendItem(Movie& m, const QString& firstName, const QString& lastName,
                     const QDate& due, const QDateTime& when)
{
    m.loan.loaned        = true;
    m.loan.due           = due;
    m.loan.userFirstName = firstName;
    m.loan.userLastName  = lastName;

    Event e;
    e.type          = loanEventBorrowed();
    e.timestamp     = when;
    e.userFirstName = firstName;
    e.userLastName  = lastName;
    m.events.append(e);
}

// No-op when the item isn't currently lent out, so a double click on "Return"
// cannot append a second event or invent a borrower.
inline void returnItem(Movie& m, const QDateTime& when)
{
    if (!m.loan.loaned) return;

    Event e;
    e.type          = loanEventReturned();
    e.timestamp     = when;
    e.userFirstName = m.loan.userFirstName;
    e.userLastName  = m.loan.userLastName;
    m.events.append(e);

    m.loan = LoanInfo{};
}

} // namespace xyz
