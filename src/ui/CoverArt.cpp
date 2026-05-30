#include "ui/CoverArt.h"

#include "ui/Theme.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace xyz {
namespace CoverArt {

QPixmap placeholder(const QString& title, int year, const QString& format,
                    QSize size, bool withText, qreal dpr)
{
    if (size.isEmpty()) return {};

    QPixmap pm(size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r(0, 0, size.width(), size.height());
    const qreal radius = qMin<qreal>(6.0, size.width() * 0.05 + 2.0);

    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.setClipPath(clip);

    // --- Gradient body (linear, 150deg ~ top-left → bottom-right) ----------
    QColor c1, c2;
    Theme::coverGradientColors(title, c1, c2);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0.0, c1);
    g.setColorAt(1.0, c2);
    p.fillPath(clip, g);

    // --- Filmstrip perforation (xp-cover-perf) -----------------------------
    const qreal perfW = qMax<qreal>(6.0, size.width() * 0.06);
    p.fillRect(QRectF(0, 0, perfW, r.height()), QColor(255, 255, 255, 18));
    p.setPen(QPen(QColor(255, 255, 255, 36), 1));
    p.drawLine(QPointF(perfW, 0), QPointF(perfW, r.height()));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 40));
    const qreal hole = perfW * 0.5;
    for (qreal y = 4; y < r.height() - hole; y += hole * 2.0)
        p.drawRoundedRect(QRectF((perfW - hole) / 2.0, y, hole, hole * 0.9), 1, 1);

    if (withText) {
        const qreal pad = qMax<qreal>(10.0, size.width() * 0.08);

        // Year + title anchored to the bottom (xp-cover-body).
        p.setPen(QColor(255, 255, 255, 235));
        QFont yf;
        yf.setPointSizeF(qMax<qreal>(7.0, size.height() * 0.035));
        yf.setBold(true);
        p.setFont(yf);
        const QFontMetricsF yfm(yf);

        QFont tf;
        tf.setPointSizeF(qMax<qreal>(9.0, size.height() * 0.058));
        tf.setBold(true);
        const QFontMetricsF tfm(tf);

        const qreal textW = r.width() - pad - 8;
        // Wrap the title to at most 3 lines.
        QStringList lines;
        {
            QString cur;
            const QStringList words = title.split(QChar(u' '), Qt::SkipEmptyParts);
            for (const QString& w : words) {
                const QString trial = cur.isEmpty() ? w : cur + QChar(u' ') + w;
                if (tfm.horizontalAdvance(trial) > textW && !cur.isEmpty()) {
                    lines << cur;
                    cur = w;
                    if (lines.size() == 2) break;
                } else {
                    cur = trial;
                }
            }
            if (lines.size() < 3 && !cur.isEmpty())
                lines << tfm.elidedText(cur, Qt::ElideRight, textW);
            if (lines.isEmpty()) lines << tfm.elidedText(title, Qt::ElideRight, textW);
        }

        qreal y = r.height() - pad;
        for (int i = lines.size() - 1; i >= 0; --i) {
            p.setFont(tf);
            p.drawText(QPointF(pad, y), lines[i]);
            y -= tfm.height();
        }
        if (year > 0) {
            p.setFont(yf);
            p.setPen(QColor(255, 255, 255, 180));
            p.drawText(QPointF(pad, y - 2), QString::number(year));
        }

        // Format chip (xp-cover-fmt), top-right.
        if (!format.isEmpty()) {
            const QString label = Theme::formatBadge(format).label;
            QFont ff;
            ff.setPointSizeF(qMax<qreal>(6.0, size.height() * 0.03));
            ff.setBold(true);
            p.setFont(ff);
            const QFontMetricsF ffm(ff);
            const qreal cw = ffm.horizontalAdvance(label) + 10;
            const qreal ch = ffm.height() + 4;
            const QRectF chip(r.width() - cw - 6, 6, cw, ch);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 115));
            p.drawRoundedRect(chip, 3, 3);
            p.setPen(QColor(255, 255, 255, 235));
            p.drawText(chip, Qt::AlignCenter, label);
        }
    }

    p.end();
    return pm;
}

} // namespace CoverArt
} // namespace xyz
