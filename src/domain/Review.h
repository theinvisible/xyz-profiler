#pragma once

namespace xyz {

// User's own star ratings for an item. DP4 ships these as four integer
// attributes on a self-closing <Review/> element. Range 0-10 in practice
// (0 = unrated).
struct Review {
    int film   = 0;
    int video  = 0;
    int audio  = 0;
    int extras = 0;
};

} // namespace xyz
