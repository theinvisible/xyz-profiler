#pragma once

#include <QDate>
#include <QString>

namespace xyz {

// Tracks whether the item is currently lent out and to whom.
// DP4's <LoanInfo> ships a Loaned bool, optional Due date, and a User
// element with FirstName/LastName/EmailAddress/PhoneNumber attributes.
struct LoanInfo {
    bool    loaned = false;
    QDate   due;
    QString userFirstName;
    QString userLastName;
    QString userEmail;
    QString userPhone;
};

} // namespace xyz
