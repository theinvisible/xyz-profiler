#pragma once

#include <QString>

namespace xyz {

// User-defined name/value pair carried through unchanged from the source export.
// Profiler 4 stores arbitrary key/value metadata under <CustomFields>.
struct CustomField {
    QString name;
    QString value;
};

} // namespace xyz
