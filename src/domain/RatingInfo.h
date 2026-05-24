#pragma once

#include <QString>

namespace xyz {

// Age/content rating shipped by DP4 as five flat sibling elements.
// Equally applicable to films (MPAA, FSK, BBFC) and games (USK, PEGI).
struct RatingInfo {
    QString system;     // "Film", "Film & Television", "USK", ...
    QString value;      // "R", "FSK-18/KJ", "PG-13", ...
    int     age = 0;    // minimum age in years; 0 = unrated/unknown
    int     variant = 0;
    QString details;    // free-text explanation
};

} // namespace xyz
