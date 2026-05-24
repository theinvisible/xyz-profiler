#pragma once

#include "importers/Importer.h"

#include <QString>

class QIODevice;

namespace xyz {

// Importer for DVD Profiler 4's <Collection><DVD>...</DVD></Collection>
// XML export format. One-way: this maps the legacy export into
// xyz's canonical `Movie` structs. No writes back to DP4.
//
// Optionally resolves disc cover image paths against an `Images/`
// directory that DP4 dumps next to the XML (files named `<id>f.jpg` and
// `<id>b.jpg`). Use `setImagesDirectory()` before calling `importFile()`.
class DvdProfilerXmlImporter final : public Importer {
public:
    DvdProfilerXmlImporter() = default;
    explicit DvdProfilerXmlImporter(QString imagesDirectory);

    void setImagesDirectory(const QString& dir) { m_imagesDir = dir; }
    QString imagesDirectory() const { return m_imagesDir; }

    QString id() const override { return QStringLiteral("dvdprofiler-xml"); }
    QString displayName() const override { return QStringLiteral("DVD Profiler 4 (Collection.xml)"); }
    QStringList filePatterns() const override { return {QStringLiteral("Collection.xml"), QStringLiteral("*.xml")}; }

    Result importFile(const QString& path) override;

    // Lower-level entry point — useful for tests / in-memory buffers.
    Result importDevice(QIODevice* device);

private:
    QString m_imagesDir;
};

} // namespace xyz
