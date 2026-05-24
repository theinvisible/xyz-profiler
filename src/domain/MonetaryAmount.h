#pragma once

#include <QString>

namespace xyz {

// Monetary value plus the currency metadata DP4 ships alongside.
//
// `value` is the numeric body (string so it round-trips losslessly — DP4
// emits "0", "14.99", "12,99", etc. depending on locale). Reused by both
// <PurchasePrice> and <SRP>, which share the same XML attribute shape.
//
// `formattedValue` is the original locale-formatted display string (e.g.
// "€14,99", "$30.95") — keep it verbatim so the UI can show the user's
// own preferred formatting without a re-parse.
struct MonetaryAmount {
    QString value;
    QString denominationType;         // ISO-like code: "EUR", "USD"
    QString denominationDescription;  // long label: "Europe (Euro)"
    QString formattedValue;
};

} // namespace xyz
