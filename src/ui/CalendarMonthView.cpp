#include "ui/CalendarMonthView.h"

#include "ui/CalendarBuckets.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

namespace xyz {

CalendarMonthView::CalendarMonthView(QWidget* parent)
    : QWidget(parent), m_today(QDate::currentDate())
{
    setMouseTracking(true);
    setMinimumHeight(380);
}

void CalendarMonthView::setData(const CalendarBuckets* buckets, const CoverIndex* covers)
{
    m_buckets = buckets;
    m_covers  = covers;
    update();
}

void CalendarMonthView::setMonth(int year, int month)
{
    m_year  = year;
    m_month = qBound(1, month, 12);
    update();
}

QDate CalendarMonthView::gridStart_() const
{
    const QDate first(m_year, m_month, 1);
    return first.addDays(-(first.dayOfWeek() - 1));   // back to Monday
}

QRect CalendarMonthView::cellRect_(int index) const
{
    const int col = index % 7, row = index / 7;
    const int hh = headerH_();
    const int gridH = height() - hh;
    const int x0 = col * width() / 7;
    const int x1 = (col + 1) * width() / 7;
    const int y0 = hh + row * gridH / 6;
    const int y1 = hh + (row + 1) * gridH / 6;
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

int CalendarMonthView::cellAt_(const QPoint& pt) const
{
    const int hh = headerH_();
    const int gridH = height() - hh;
    if (pt.y() < hh || gridH <= 0 || width() <= 0) return -1;
    int col = pt.x() * 7 / width();
    int row = (pt.y() - hh) * 6 / gridH;
    col = qBound(0, col, 6);
    row = qBound(0, row, 5);
    return row * 7 + col;
}

void CalendarMonthView::paintEvent(QPaintEvent*)
{
    const Palette& pal = Theme::current();
    const qreal dpr = devicePixelRatioF();
    m_hits.clear();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), pal.win);

    // Weekday header (Mon..Sun, localized short names).
    {
        QFont hf = p.font();
        hf.setBold(true);
        hf.setPointSizeF(qMax(7.5, hf.pointSizeF() - 1.0));
        p.setFont(hf);
        p.setPen(pal.text3);
        const QLocale loc;
        for (int col = 0; col < 7; ++col) {
            const QString name = loc.standaloneDayName(col + 1, QLocale::ShortFormat);
            const int x0 = col * width() / 7;
            const int x1 = (col + 1) * width() / 7;
            p.drawText(QRect(x0, 0, x1 - x0, headerH_()), Qt::AlignCenter, name);
        }
    }

    const QList<QString> empty;
    const QDate start = gridStart_();
    for (int i = 0; i < 42; ++i) {
        const QDate date = start.addDays(i);
        const bool dimmed = (date.month() != m_month) || (date.year() != m_year);
        const QRect cell = cellRect_(i);
        const QList<QString>& ids =
            (dimmed || !m_buckets) ? empty : m_buckets->day(date);

        const int maxThumbs = qMax(1, (cell.width() - 14) / 28);
        paintHybridCell(&p, cell, QString::number(date.day()), ids,
                        m_buckets ? m_buckets->maxDayCount : 0,
                        m_covers ? *m_covers : CoverIndex(),
                        maxThumbs, QSize(24, 36),
                        date == m_today, dimmed, i == m_hover, dpr, &m_hits);
    }
}

void CalendarMonthView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }

    for (const ThumbHit& h : m_hits) {
        if (h.rect.contains(e->pos())) {
            emit movieActivated(h.id);
            return;
        }
    }
    const int idx = cellAt_(e->pos());
    if (idx < 0 || !m_buckets) return;
    const QDate date = gridStart_().addDays(idx);
    if (date.month() != m_month || date.year() != m_year) return;
    if (!m_buckets->day(date).isEmpty())
        emit dayActivated(date);
}

void CalendarMonthView::mouseMoveEvent(QMouseEvent* e)
{
    const int idx = cellAt_(e->pos());
    if (idx != m_hover) {
        m_hover = idx;
        update();
    }
    bool overThumb = false;
    for (const ThumbHit& h : m_hits)
        if (h.rect.contains(e->pos())) { overThumb = true; break; }
    setCursor(overThumb ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void CalendarMonthView::leaveEvent(QEvent*)
{
    if (m_hover != -1) { m_hover = -1; update(); }
}

} // namespace xyz
