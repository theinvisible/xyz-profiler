#pragma once

#include "domain/Movie.h"

#include <QDate>
#include <QHash>
#include <QList>
#include <QString>

namespace xyz {

// Which date drives the calendar: the film's release date or when the user
// bought it. Chosen by the user at the top of the Calendar window.
enum class DateBasis { Release, Purchase };

// Aggregates a movie list by date for the Calendar views. Pure value type — no
// Qt widgets — so it is unit-testable in isolation (see test_calendar_buckets).
//
// Rules:
//   * Box-set parent placeholders are skipped (they duplicate their children,
//     mirroring the cover grid's IsBoxSetParentRole filter).
//   * Release basis: a movie with no precise releaseDate but a productionYear
//     is counted at YEAR granularity only (yearOnlyCount) — it appears in the
//     timeline/year view but not on a specific day.
//   * A movie with no usable date for the chosen basis is "undated".
//
// byDay is keyed by Julian day number (QDate::toJulianDay) rather than QDate so
// the hash works regardless of the Qt version's qHash(QDate) availability.
struct CalendarBuckets {
    QHash<qint64, QList<QString>> byDay;     // julianDay  -> movie ids
    QHash<int,    QList<QString>> byMonth;   // year*100+mo -> movie ids
    QHash<int,    int>            byYear;    // year        -> count

    int undatedCount  = 0;   // no usable date at all
    int yearOnlyCount = 0;   // release basis: only a productionYear, no day/month

    int minYear = 0;         // 0 when there is no dated data at all
    int maxYear = 0;

    int maxDayCount   = 0;   // peak counts, for heatmap intensity scaling
    int maxMonthCount = 0;
    int maxYearCount  = 0;

    // True when nothing could be placed (empty library or no usable dates).
    bool isEmpty() const { return minYear == 0 && undatedCount == 0; }

    static int monthKey(int year, int month) { return year * 100 + month; }

    // Lookups — return a shared empty list when the bucket is absent.
    const QList<QString>& day(const QDate& d) const;
    const QList<QString>& month(int year, int month) const;
    int yearCount(int year) const { return byYear.value(year, 0); }
};

// Build the buckets for `movies` under the chosen `basis`. O(N) over the list.
CalendarBuckets buildBuckets(const QList<Movie>& movies, DateBasis basis);

} // namespace xyz
