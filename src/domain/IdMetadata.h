#pragma once

#include <QString>

namespace xyz {

// Parsed-out identifier metadata. DP4 stores the UPC/EAN both as the raw
// `<ID>` (potentially with a variant suffix like "X.5") and as a set of
// pre-parsed sibling elements giving the base UPC, variant number, and
// regional locality.
//
// Useful for de-duplication (`base + localityId` uniquely identifies a
// release across reprints) and for displaying a friendly origin in the UI.
struct IdMetadata {
    QString base;             // numeric UPC without the variant suffix
    int     variantNum = 0;
    int     localityId = 0;
    QString localityDescription;  // "Germany", "United States", ...
    QString type;             // "UPCEAN", ...
};

} // namespace xyz
