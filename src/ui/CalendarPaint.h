#pragma once

#include <QHash>
#include <QList>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QString>

class QPainter;

namespace xyz {

// Lightweight view-model for a movie as the calendar cells need it: just the
// cover path + the bits required to render a placeholder. Built once per rebuild
// by CalendarWindow and shared (by const ptr) with the views.
struct CoverRef {
    QString path;
    QString title;
    int     year = 0;
};
using CoverIndex = QHash<QString, CoverRef>;

// A painted mini-cover's rect paired with its movie id, for click hit-testing.
struct ThumbHit {
    QRect   rect;
    QString id;
};

// Cached mini cover for one movie (scaled+cropped real scan, or a placeholder).
// Mirrors CoverDelegate's scaling and CoverCache keying.
QPixmap calendarMiniCover(const CoverRef& ref, QSize size, qreal dpr);

// Paint one "hybrid" calendar cell: a count-tinted rounded panel, a top-left
// label (day number / month name), a count pill top-right, and up to `maxThumbs`
// mini covers along the bottom with a "+N" overflow chip. Appends each painted
// thumbnail's rect+id to `hitsOut` (when non-null) for hit-testing.
void paintHybridCell(QPainter* p, const QRect& cell, const QString& topLabel,
                     const QList<QString>& ids, int maxCount,
                     const CoverIndex& covers, int maxThumbs, QSize thumb,
                     bool today, bool dimmed, bool hovered, qreal dpr,
                     QList<ThumbHit>* hitsOut);

} // namespace xyz
