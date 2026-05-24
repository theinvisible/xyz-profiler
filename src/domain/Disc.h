#pragma once

#include <QString>

namespace xyz {

// One physical disc inside a release. DVD Profiler 4 models discs with
// distinct side-A / side-B identity because of dual-sided DVDs (flipper
// discs); modern formats only ever populate the side-A fields.
//
// All fields are optional. `dualSided` is the disc-level flag; the per-side
// `dualLayeredSideA/B` flags are about layering, not sides.
struct Disc {
    QString descriptionSideA;
    QString descriptionSideB;
    QString discIdSideA;
    QString discIdSideB;
    QString labelSideA;
    QString labelSideB;
    bool    dualLayeredSideA = false;
    bool    dualLayeredSideB = false;
    bool    dualSided        = false;
    QString location;        // user-managed physical location label
    QString slot;            // user-managed slot label
};

} // namespace xyz
