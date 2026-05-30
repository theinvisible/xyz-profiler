#pragma once

#include <QPixmap>
#include <QSize>
#include <QString>

namespace xyz {

// ---------------------------------------------------------------------------
// CoverArt — placeholder poster art (xp-cover) for titles without real cover
// scans. Mirrors the Claude Design `Cover` component: a deterministic two-stop
// gradient with a filmstrip-perforation left edge, the year, the title, and a
// small format chip. Used by the cover grid, the list row swatch and the
// detail pane so placeholders look identical everywhere.
// ---------------------------------------------------------------------------
namespace CoverArt {

// Full placeholder poster. `withText` paints the year/title/format overlay
// (off for tiny swatches). `dpr` is the device pixel ratio of the target.
QPixmap placeholder(const QString& title, int year, const QString& format,
                    QSize size, bool withText, qreal dpr = 1.0);

} // namespace CoverArt
} // namespace xyz
