#pragma once

#include "domain/Movie.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace xyz {

// Abstract base for source-format importers.
//
// The importer pipeline is intentionally narrow: a concrete importer reads
// some external representation (DP4 XML, a CSV, a JSON dump, another DB)
// and returns a Result carrying a list of canonical `Movie` structs plus
// an error string. The persistence layer is downstream and is agnostic to
// the source format — it consumes only the canonical types from
// `xyz_domain`.
//
// Each importer lives in its own subdirectory under `src/importers/` so
// that adding a new source format is a matter of dropping a sibling
// module beside `dvdprofiler/`, implementing this interface, and wiring it
// into the UI's format picker.
class Importer {
public:
    struct Result {
        QList<Movie> movies;
        QString      errorString;
        bool         ok = true;
    };

    virtual ~Importer() = default;

    // Stable identifier for settings / registries.
    // e.g. "dvdprofiler-xml". Lowercase, kebab-case, no spaces.
    virtual QString id() const = 0;

    // Human-readable name for UI pickers.
    // e.g. "DVD Profiler 4 (Collection.xml)".
    virtual QString displayName() const = 0;

    // File patterns the importer recognises, for QFileDialog filters.
    // e.g. {"Collection.xml", "*.xml"}.
    virtual QStringList filePatterns() const = 0;

    // Run the import. Implementation-specific extra paths (e.g. an images
    // directory) are passed via subclass constructor / setters, so the
    // base signature stays simple.
    virtual Result importFile(const QString& path) = 0;
};

} // namespace xyz
