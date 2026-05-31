#pragma once

#include "ui/CalendarPaint.h"

#include <QDate>
#include <QList>
#include <QWidget>

namespace xyz {

struct CalendarBuckets;

// Custom-painted Monday-start month grid (6 rows × 7 cols). Each in-month day is
// a hybrid cell (count tint + mini covers + count/overflow). Adjacent-month days
// render dimmed and empty. Clicking a mini cover activates that film; clicking a
// populated day's overflow/background activates the day (for the day list).
class CalendarMonthView : public QWidget {
    Q_OBJECT

public:
    explicit CalendarMonthView(QWidget* parent = nullptr);

    void setData(const CalendarBuckets* buckets, const CoverIndex* covers);
    void setMonth(int year, int month);

    QSize minimumSizeHint() const override { return {600, 380}; }

signals:
    void movieActivated(const QString& id);
    void dayActivated(const QDate& date);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int   headerH_() const { return 26; }
    QDate gridStart_() const;        // first visible cell (a Monday)
    QRect cellRect_(int index) const;
    int   cellAt_(const QPoint& p) const;   // 0..41, or -1

    const CalendarBuckets* m_buckets = nullptr;
    const CoverIndex*      m_covers  = nullptr;
    int   m_year  = 2000;
    int   m_month = 1;
    QDate m_today;
    int   m_hover = -1;
    QList<ThumbHit> m_hits;          // rebuilt each paint, for click hit-testing
};

} // namespace xyz
