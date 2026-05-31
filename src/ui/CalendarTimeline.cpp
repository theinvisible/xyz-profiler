#include "ui/CalendarTimeline.h"

#include "ui/CalendarBuckets.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <limits>

namespace xyz {

CalendarTimeline::CalendarTimeline(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(64);
}

void CalendarTimeline::setData(const CalendarBuckets* buckets)
{
    m_buckets = buckets;
    update();
}

void CalendarTimeline::setCurrentYear(int year)
{
    if (m_currentYear == year) return;
    m_currentYear = year;
    update();
}

int CalendarTimeline::yearAt_(int x) const
{
    if (!m_buckets || m_buckets->minYear == 0 || width() <= 0)
        return std::numeric_limits<int>::min();
    const int n = m_buckets->maxYear - m_buckets->minYear + 1;
    int idx = x * n / width();
    idx = qBound(0, idx, n - 1);
    return m_buckets->minYear + idx;
}

void CalendarTimeline::paintEvent(QPaintEvent*)
{
    const Palette& pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), pal.win);

    // Track panel behind the bars.
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath bgPath;
    bgPath.addRoundedRect(track, 8, 8);
    p.fillPath(bgPath, pal.panel2);
    p.setPen(QPen(pal.border, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(track, 8, 8);

    if (!m_buckets || m_buckets->minYear == 0) {
        p.setPen(pal.text3);
        p.drawText(rect(), Qt::AlignCenter, tr("No dated titles"));
        return;
    }

    const int n = m_buckets->maxYear - m_buckets->minYear + 1;
    const int maxCount = qMax(1, m_buckets->maxYearCount);
    const int areaH = height() - labelsH_() - 10;
    const int baseY = height() - labelsH_() - 4;

    // Decide a label step that avoids overlap (~36px per label).
    const double slotW = double(width()) / n;
    const int labelStep = qMax(1, int(36.0 / qMax(1.0, slotW)) + (slotW < 36 ? 1 : 0));

    QFont lf = p.font();
    lf.setPointSizeF(qMax(7.0, lf.pointSizeF() - 1.5));

    for (int i = 0; i < n; ++i) {
        const int year  = m_buckets->minYear + i;
        const int count = m_buckets->yearCount(year);
        const int x0 = i * width() / n;
        const int x1 = (i + 1) * width() / n;
        const int slot = x1 - x0;
        const int bw = qMax(3, slot - 4);
        const int bx = x0 + (slot - bw) / 2;

        const bool isCur   = (year == m_currentYear);
        const bool isHover = (year == m_hoverYear);

        if (count > 0) {
            int bh = int(double(count) / maxCount * areaH);
            bh = qMax(bh, 3);
            const QRectF bar(bx, baseY - bh, bw, bh);
            QColor c = pal.accent;
            if (isHover) c = pal.accentHover.lighter(125);
            QPainterPath bp;
            bp.addRoundedRect(bar, 2.5, 2.5);
            p.fillPath(bp, c);
        }

        if (isCur) {
            // Highlight the current year's whole slot.
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(pal.accent.red(), pal.accent.green(), pal.accent.blue(), 38));
            p.drawRoundedRect(QRectF(x0 + 1, 4, slot - 2, height() - 8), 4, 4);
        }

        if (i % labelStep == 0) {
            p.setFont(lf);
            p.setPen(isCur ? pal.text : pal.text3);
            p.drawText(QRect(x0, height() - labelsH_(), slot, labelsH_()),
                       Qt::AlignCenter, QString::number(year));
        }
    }
}

void CalendarTimeline::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    const int year = yearAt_(int(e->position().x()));
    if (year != std::numeric_limits<int>::min())
        emit yearActivated(year);
}

void CalendarTimeline::mouseMoveEvent(QMouseEvent* e)
{
    const int year = yearAt_(int(e->position().x()));
    if (year == std::numeric_limits<int>::min()) return;
    if (year != m_hoverYear) {
        m_hoverYear = year;
        update();
    }
    setCursor(Qt::PointingHandCursor);
    const int count = m_buckets ? m_buckets->yearCount(year) : 0;
    QToolTip::showText(e->globalPosition().toPoint(),
                       tr("%1 · %n film(s)", nullptr, count).arg(year), this);
}

void CalendarTimeline::leaveEvent(QEvent*)
{
    if (m_hoverYear != 0) { m_hoverYear = 0; update(); }
    setCursor(Qt::ArrowCursor);
}

} // namespace xyz
