#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace xyz {

// ---------------------------------------------------------------------------
// IconFactory — line icons rendered from the Claude Design `ICONS` path set.
// ---------------------------------------------------------------------------
// The design used inline SVG (24x24 viewBox, round caps/joins, 1.6 stroke).
// We rebuild each icon as an SVG document with the stroke (and, for the star,
// fill) injected from the wanted colour, then rasterise it via QSvgRenderer.
// Colour comes from the active theme, so callers re-request icons on theme
// change to keep them tinted correctly.
// ---------------------------------------------------------------------------
namespace IconFactory {

// Known icon names: add, edit, trash, search, list, grid, sun, moon,
// settings, star, disc, film, chevron, close, sort, shelf, user, check,
// filter, refresh, play, import.
QPixmap pixmap(const QString& name, const QColor& color,
               int size = 18, qreal stroke = 1.6, qreal dpr = 1.0);

QIcon   icon(const QString& name, const QColor& color,
             int size = 18, qreal stroke = 1.6);

} // namespace IconFactory
} // namespace xyz
