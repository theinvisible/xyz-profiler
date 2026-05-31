#include "ui/CalendarBuckets.h"

#include <algorithm>

namespace xyz {

const QList<QString>& CalendarBuckets::day(const QDate& d) const
{
    static const QList<QString> kEmpty;
    if (!d.isValid()) return kEmpty;
    const auto it = byDay.constFind(d.toJulianDay());
    return it != byDay.constEnd() ? it.value() : kEmpty;
}

const QList<QString>& CalendarBuckets::month(int year, int month) const
{
    static const QList<QString> kEmpty;
    const auto it = byMonth.constFind(monthKey(year, month));
    return it != byMonth.constEnd() ? it.value() : kEmpty;
}

CalendarBuckets buildBuckets(const QList<Movie>& movies, DateBasis basis)
{
    CalendarBuckets b;

    const auto noteYear = [&b](int y) {
        if (b.minYear == 0 || y < b.minYear) b.minYear = y;
        if (y > b.maxYear) b.maxYear = y;
    };

    for (const Movie& m : movies) {
        if (m.boxSet.isParent) continue;   // set placeholder — counted via children

        const QDate date = (basis == DateBasis::Release) ? m.releaseDate
                                                          : m.purchase.date;
        if (date.isValid()) {
            const int y = date.year();
            b.byDay[date.toJulianDay()].append(m.id);
            b.byMonth[CalendarBuckets::monthKey(y, date.month())].append(m.id);
            ++b.byYear[y];
            noteYear(y);
        } else if (basis == DateBasis::Release && m.productionYear > 0) {
            const int y = m.productionYear;
            ++b.byYear[y];
            ++b.yearOnlyCount;
            noteYear(y);
        } else {
            ++b.undatedCount;
        }
    }

    for (auto it = b.byDay.constBegin(); it != b.byDay.constEnd(); ++it)
        b.maxDayCount = std::max(b.maxDayCount, int(it.value().size()));
    for (auto it = b.byMonth.constBegin(); it != b.byMonth.constEnd(); ++it)
        b.maxMonthCount = std::max(b.maxMonthCount, int(it.value().size()));
    for (auto it = b.byYear.constBegin(); it != b.byYear.constEnd(); ++it)
        b.maxYearCount = std::max(b.maxYearCount, it.value());

    return b;
}

} // namespace xyz
