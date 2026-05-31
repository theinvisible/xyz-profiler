#include "ui/CalendarPaint.h"

#include "ui/CoverArt.h"
#include "ui/CoverCache.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace xyz {
namespace {

QColor blend(const QColor& a, const QColor& b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor(int(a.red()   + (b.red()   - a.red())   * t),
                  int(a.green() + (b.green() - a.green()) * t),
                  int(a.blue()  + (b.blue()  - a.blue())  * t));
}

QString sizeTag(QSize s, qreal dpr)
{
    return QString::number(s.width()) + u'x' + QString::number(s.height())
         + u'@' + QString::number(dpr);
}

} // namespace

QPixmap calendarMiniCover(const CoverRef& ref, QSize size, qreal dpr)
{
    QPixmap pm;
    if (!ref.path.isEmpty()) {
        const QString key = CoverCache::key(ref.path,
                                            QStringLiteral("calmini_") + sizeTag(size, dpr));
        if (QPixmapCache::find(key, &pm)) return pm;
        QPixmap raw(ref.path);
        if (!raw.isNull()) {
            const QSize target = size * dpr;
            QPixmap scaled = raw.scaled(target, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
            const int dx = (scaled.width()  - target.width())  / 2;
            const int dy = (scaled.height() - target.height()) / 2;
            pm = scaled.copy(qMax(0, dx), qMax(0, dy), target.width(), target.height());
            pm.setDevicePixelRatio(dpr);
            QPixmapCache::insert(key, pm);
            return pm;
        }
    }
    // Placeholder gradient (no text — these thumbs are tiny).
    const QString phKey = QStringLiteral("calph_") + ref.title + u'_'
                        + QString::number(ref.year) + u'_' + sizeTag(size, dpr);
    if (!QPixmapCache::find(phKey, &pm)) {
        pm = CoverArt::placeholder(ref.title, ref.year, QString(), size, false, dpr);
        QPixmapCache::insert(phKey, pm);
    }
    return pm;
}

void paintHybridCell(QPainter* p, const QRect& cell, const QString& topLabel,
                     const QList<QString>& ids, int maxCount,
                     const CoverIndex& covers, int maxThumbs, QSize thumb,
                     bool today, bool dimmed, bool hovered, qreal dpr,
                     QList<ThumbHit>* hitsOut)
{
    const Palette& pal = Theme::current();
    const int count = int(ids.size());
    const int pad = 6;
    const qreal radius = 8.0;
    const QRectF r = QRectF(cell).adjusted(2.5, 2.5, -2.5, -2.5);

    // --- Background: count-tinted panel -----------------------------------
    QColor bg = pal.panel2;
    if (count > 0 && maxCount > 0) {
        const qreal t = qMin(1.0, double(count) / double(maxCount));
        bg = blend(pal.panel2, pal.accent, 0.14 + 0.34 * t);
    }
    if (dimmed)
        bg = blend(pal.win, pal.panel2, 0.4);
    if (hovered)
        bg = blend(bg, pal.text, 0.06);

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p->fillPath(path, bg);

    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(today ? pal.accent : pal.border, today ? 1.6 : 1.0));
    p->drawRoundedRect(r, radius, radius);

    // --- Top-left label (day number / month name) -------------------------
    {
        QFont lf = p->font();
        lf.setBold(true);
        p->setFont(lf);
        p->setPen(dimmed ? pal.text3 : pal.text2);
        p->drawText(QRectF(r.left() + pad, r.top() + 4, r.width() - 2 * pad, 18),
                    Qt::AlignLeft | Qt::AlignVCenter, topLabel);
    }

    // --- Count pill (top-right) -------------------------------------------
    if (count > 0) {
        QFont cf = p->font();
        cf.setBold(true);
        cf.setPointSizeF(qMax(7.0, cf.pointSizeF() - 1.5));
        p->setFont(cf);
        const QString ct = QString::number(count);
        const QFontMetrics fm(cf);
        const int w = qMax(16, fm.horizontalAdvance(ct) + 10);
        const QRectF pill(r.right() - pad - w, r.top() + 5, w, 16);
        p->setPen(Qt::NoPen);
        p->setBrush(pal.accent);
        p->drawRoundedRect(pill, 8, 8);
        p->setPen(pal.accentFg);
        p->drawText(pill, Qt::AlignCenter, ct);
    }

    // --- Mini covers + overflow chip (bottom) -----------------------------
    if (count > 0) {
        const int shown = qMin(count, maxThumbs);
        const bool overflow = count > maxThumbs;
        const int gap = 4;
        qreal x = r.left() + pad;
        const qreal y = r.bottom() - pad - thumb.height();

        for (int i = 0; i < shown; ++i) {
            const QRect tr(int(x), int(y), thumb.width(), thumb.height());
            const QPixmap mp = calendarMiniCover(covers.value(ids[i]), thumb, dpr);
            p->save();
            QPainterPath clip;
            clip.addRoundedRect(QRectF(tr), 3, 3);
            p->setClipPath(clip);
            p->drawPixmap(tr, mp);
            p->restore();
            p->setPen(QPen(QColor(0, 0, 0, 70), 1));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(tr).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
            if (hitsOut) hitsOut->append({tr, ids[i]});
            x += thumb.width() + gap;
        }

        if (overflow) {
            QFont sf = p->font();
            sf.setBold(true);
            sf.setPointSizeF(qMax(7.0, sf.pointSizeF() - 1.5));
            p->setFont(sf);
            const QString more = QStringLiteral("+%1").arg(count - shown);
            const QFontMetrics fm(sf);
            const int w = qMax(22, fm.horizontalAdvance(more) + 10);
            const QRectF chip(x, y + thumb.height() / 2.0 - 9, w, 18);
            p->setPen(Qt::NoPen);
            p->setBrush(pal.panel3);
            p->drawRoundedRect(chip, 5, 5);
            p->setPen(pal.text2);
            p->drawText(chip, Qt::AlignCenter, more);
        }
    }
}

} // namespace xyz
