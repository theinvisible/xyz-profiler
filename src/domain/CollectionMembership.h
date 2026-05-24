#pragma once

#include <QString>

namespace xyz {

// Whether the item is owned, on a wishlist, or something else.
// DP4 ships this as the text body of <CollectionType> with an
// `IsPartOfOwnedCollection` boolean attribute.
struct CollectionMembership {
    QString type;                          // "Owned", "Wishlist", ...
    bool    isPartOfOwnedCollection = true;
};

} // namespace xyz
