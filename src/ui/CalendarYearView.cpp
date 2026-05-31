#include "ui/CalendarYearView.h"

#include "ui/CalendarBuckets.h"
#include "ui/Theme.h"

#include <QDate>
#include <QEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

namespace xyz {

CalendarYearView::CalendarYearView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(380);
}

void CalendarYearView::setData(const CalendarBuckets* buckets, const CoverIndex* covers)
{
    m_buckets = buckets;
    m_covers  = covers;
    update();
}

void CalendarYearView::setYear(int year)
{
    m_year = year;
    update();
}

QRect CalendarYearView::cellRect_(int index) const
{
    const int col = index % 4, row = index / 4;
    const int x0 = col * width() / 4;
    const int x1 = (col + 1) * width() / 4;
    const int y0 = row * height() / 3;
    const int y1 = (row + 1) * height() / 3;
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

int CalendarYearView::cellAt_(const QPoint& pt) const
{
    if (width() <= 0 || height() <= 0) return -1;
    int col = pt.x() * 4 / width();
    int row = pt.y() * 3 / height();
    col = qBound(0, col, 3);
    row = qBound(0, row, 2);
    return row * 4 + col;
}

void CalendarYearView::paintEvent(QPaintEvent*)
{
    const Palette& pal = Theme::current();
    const qreal dpr = devicePixelRatioF();
    const QDate today = QDate::currentDate();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), pal.win);

    const QLocale loc;
    const QList<QString> empty;
    for (int m = 1; m <= 12; ++m) {
        const QRect cell = cellRect_(m - 1);
        const QList<QString>& ids = m_buckets ? m_buckets->month(m_year, m) : empty;
        const QString label = loc.standaloneMonthName(m, QLocale::LongFormat);
        const int maxThumbs = qMax(2, (cell.width() - 16) / 34);
        const bool isToday = (m_year == today.year() && m == today.month());
        // hitsOut == nullptr: covers are decorative; clicking the cell drills in.
        paintHybridCell(&p, cell, label, ids,
                        m_buckets ? m_buckets->maxMonthCount : 0,
                        m_covers ? *m_covers : CoverIndex(),
                        maxThumbs, QSize(30, 45),
                        isToday, /*dimmed*/ false, (m - 1) == m_hover, dpr, nullptr);
    }
}

void CalendarYearView::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    const int idx = cellAt_(e->pos());
    if (idx >= 0)
        emit monthActivated(m_year, idx + 1);
}

void CalendarYearView::mouseMoveEvent(QMouseEvent* e)
{
    const int idx = cellAt_(e->pos());
    if (idx != m_hover) {
        m_hover = idx;
        update();
    }
    setCursor(Qt::PointingHandCursor);
}

void CalendarYearView::leaveEvent(QEvent*)
{
    if (m_hover != -1) { m_hover = -1; update(); }
    setCursor(Qt::ArrowCursor);
}

} // namespace xyz
