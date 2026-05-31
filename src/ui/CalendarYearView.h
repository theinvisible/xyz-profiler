#pragma once

#include "ui/CalendarPaint.h"

#include <QList>
#include <QWidget>

namespace xyz {

struct CalendarBuckets;

// Custom-painted 4×3 month grid for one year. Each month is a hybrid cell
// (count tint + a few preview covers + count). Clicking a month drills into the
// month view (films are clicked there, not here).
class CalendarYearView : public QWidget {
    Q_OBJECT

public:
    explicit CalendarYearView(QWidget* parent = nullptr);

    void setData(const CalendarBuckets* buckets, const CoverIndex* covers);
    void setYear(int year);

    QSize minimumSizeHint() const override { return {600, 380}; }

signals:
    void monthActivated(int year, int month);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QRect cellRect_(int index) const;        // index 0..11 (month-1)
    int   cellAt_(const QPoint& p) const;     // 0..11, or -1

    const CalendarBuckets* m_buckets = nullptr;
    const CoverIndex*      m_covers  = nullptr;
    int m_year  = 2000;
    int m_hover = -1;
};

} // namespace xyz
