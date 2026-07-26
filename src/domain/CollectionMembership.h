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

// What the collection views are currently showing.
enum class CollectionStatus { Owned, Wishlist, All };

// DP4 knows more collection types than these two ("Order", "For Sale", ...),
// so `Owned` deliberately means "not on the wishlist" rather than
// `type == "Owned"`. That keeps the two filters exhaustive: every title is
// visible under exactly one of them, and nothing can go missing from both.
inline bool isWishlistMembership(const CollectionMembership& m)
{
    if (!m.type.isEmpty())
        return m.type.compare(QLatin1String("Wishlist"), Qt::CaseInsensitive) == 0;
    // No type at all (hand-created entries, sparse exports): fall back to the
    // attribute DP4 ships alongside it.
    return !m.isPartOfOwnedCollection;
}

inline bool matchesStatus(const CollectionMembership& m, CollectionStatus status)
{
    switch (status) {
    case CollectionStatus::All:      return true;
    case CollectionStatus::Wishlist: return isWishlistMembership(m);
    case CollectionStatus::Owned:    return !isWishlistMembership(m);
    }
    return true;
}

// Stable string form for QSettings. Unknown input falls back to Owned, which
// is also the default the UI starts on.
inline QString collectionStatusKey(CollectionStatus status)
{
    switch (status) {
    case CollectionStatus::Wishlist: return QStringLiteral("wishlist");
    case CollectionStatus::All:      return QStringLiteral("all");
    case CollectionStatus::Owned:    break;
    }
    return QStringLiteral("owned");
}

inline CollectionStatus collectionStatusFromKey(const QString& key)
{
    if (key == QLatin1String("wishlist")) return CollectionStatus::Wishlist;
    if (key == QLatin1String("all"))      return CollectionStatus::All;
    return CollectionStatus::Owned;
}

} // namespace xyz
