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

// Single display rule for both the detail pane and the edit dialog.
//
// `formattedValue` is the *source's* own formatting and is preferred when
// present. It goes stale as soon as the user edits the value or the currency,
// so every editor must clear it on write — we never synthesise one ourselves,
// because we cannot know the locale conventions the source used.
inline QString displayAmount(const MonetaryAmount& amount)
{
    if (!amount.formattedValue.isEmpty()) return amount.formattedValue;
    if (amount.value.isEmpty())           return {};
    if (amount.denominationType.isEmpty()) return amount.value;
    return amount.value + QChar(u' ') + amount.denominationType;
}

} // namespace xyz
