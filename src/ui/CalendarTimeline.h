#pragma once

#include <QWidget>

namespace xyz {

struct CalendarBuckets;

// Custom-painted year histogram across the data's [minYear, maxYear]. Bar height
// is the film count for that year (release or purchase, per the chosen basis).
// The current year is highlighted; clicking a bar jumps the calendar there.
class CalendarTimeline : public QWidget {
    Q_OBJECT

public:
    explicit CalendarTimeline(QWidget* parent = nullptr);

    void setData(const CalendarBuckets* buckets);
    void setCurrentYear(int year);

    QSize sizeHint() const override { return {600, 76}; }
    QSize minimumSizeHint() const override { return {200, 64}; }

signals:
    void yearActivated(int year);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    int labelsH_() const { return 16; }
    int yearAt_(int x) const;   // INT_MIN if no data / out of range

    const CalendarBuckets* m_buckets = nullptr;
    int m_currentYear = 0;
    int m_hoverYear   = 0;
};

} // namespace xyz
