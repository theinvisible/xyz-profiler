#pragma once

#include <QString>

namespace xyz {

// Video-presentation metadata for a film release. DP4's <Format> block
// ships ~15 boolean and 2 string children; we collapse the mutually-
// exclusive boolean groups (ColorFormat, Dimensions) into single
// semantic strings to keep queries straightforward.
//
// `colorMode` values mirror DP4's ColorFormat sub-booleans:
//   "Color" | "BlackAndWhite" | "Colorized" | "Mixed" | "" (unset)
//
// `dimensions` values mirror DP4's Dimensions sub-booleans:
//   "2D" | "3DAnaglyph" | "3DBluRay" | "" (unset)
struct VideoFormat {
    QString aspectRatio;      // "2.40", "1.85", "16:9"
    QString videoStandard;    // "NTSC", "PAL"
    QString colorMode;
    QString dimensions;

    bool letterBox       = false;
    bool panAndScan      = false;
    bool fullFrame       = false;
    bool enhancedFor16x9 = false;   // DP4: Format16X9
    bool dualSided       = false;
    bool dualLayered     = false;
};

} // namespace xyz
