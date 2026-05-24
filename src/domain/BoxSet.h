#pragma once

#include <QString>
#include <QStringList>

namespace xyz {

// Box-set membership. A media item can be either:
//   - a child of a set (`parentId` populated), or
//   - a set parent (`isParent == true`, `childIds` populated), or
//   - neither (default-constructed = standalone item).
//
// Modelling both directions on every item lets repository code resolve the
// relationship without a second lookup table; the importer fills in whichever
// side the source export carries.
struct BoxSet {
    QString     parentId;
    QStringList childIds;
    bool        isParent = false;
};

} // namespace xyz
