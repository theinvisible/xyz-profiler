#include "ui/CalendarBuckets.h"

#include <QtTest>

using namespace xyz;

namespace {

Movie mk(const QString& id, int year, const QDate& release,
         const QDate& purchase, bool boxSetParent = false)
{
    Movie m;
    m.id    = id;
    m.title = id;
    m.productionYear = year;
    m.releaseDate    = release;
    m.purchase.date  = purchase;
    m.boxSet.isParent = boxSetParent;
    return m;
}

} // namespace

class TestCalendarBuckets : public QObject {
    Q_OBJECT

private slots:
    void empty_list_is_empty();
    void release_basis_buckets_by_day_month_year();
    void release_basis_year_only_fallback();
    void purchase_basis_has_no_year_fallback();
    void skips_box_set_parents();
    void tracks_range_and_max_counts();
};

void TestCalendarBuckets::empty_list_is_empty()
{
    const CalendarBuckets b = buildBuckets({}, DateBasis::Release);
    QVERIFY(b.isEmpty());
    QCOMPARE(b.minYear, 0);
    QCOMPARE(b.undatedCount, 0);
    QCOMPARE(b.yearOnlyCount, 0);
}

void TestCalendarBuckets::release_basis_buckets_by_day_month_year()
{
    const QList<Movie> movies = {
        mk(QStringLiteral("a"), 1999, QDate(1999, 3, 24), QDate()),
        mk(QStringLiteral("b"), 1999, QDate(1999, 3, 24), QDate()),   // same day
        mk(QStringLiteral("c"), 2008, QDate(2008, 7, 18), QDate()),
    };
    const CalendarBuckets b = buildBuckets(movies, DateBasis::Release);

    QCOMPARE(b.day(QDate(1999, 3, 24)).size(), 2);
    QCOMPARE(b.day(QDate(2008, 7, 18)).size(), 1);
    QCOMPARE(b.month(1999, 3).size(), 2);
    QCOMPARE(b.yearCount(1999), 2);
    QCOMPARE(b.yearCount(2008), 1);
    QCOMPARE(b.undatedCount, 0);
    QCOMPARE(b.yearOnlyCount, 0);
}

void TestCalendarBuckets::release_basis_year_only_fallback()
{
    const QList<Movie> movies = {
        mk(QStringLiteral("a"), 1994, QDate(), QDate()),   // year only, no precise date
        mk(QStringLiteral("b"), 0,    QDate(), QDate()),   // truly undated
    };
    const CalendarBuckets b = buildBuckets(movies, DateBasis::Release);

    QCOMPARE(b.yearCount(1994), 1);
    QCOMPARE(b.yearOnlyCount, 1);
    QCOMPARE(b.undatedCount, 1);
    QVERIFY(b.day(QDate(1994, 1, 1)).isEmpty());   // never placed on a day
}

void TestCalendarBuckets::purchase_basis_has_no_year_fallback()
{
    const QList<Movie> movies = {
        mk(QStringLiteral("a"), 1994, QDate(1994, 5, 1), QDate(2020, 12, 25)),
        // valid release + production year but NO purchase date -> undated on purchase basis
        mk(QStringLiteral("b"), 2001, QDate(2001, 9, 9), QDate()),
    };
    const CalendarBuckets b = buildBuckets(movies, DateBasis::Purchase);

    QCOMPARE(b.day(QDate(2020, 12, 25)).size(), 1);
    QCOMPARE(b.yearCount(2020), 1);
    QCOMPARE(b.undatedCount, 1);
    QCOMPARE(b.yearOnlyCount, 0);   // no year fallback for purchase basis
}

void TestCalendarBuckets::skips_box_set_parents()
{
    const QList<Movie> movies = {
        mk(QStringLiteral("set"), 2010, QDate(2010, 1, 1), QDate(), /*parent*/ true),
        mk(QStringLiteral("child"), 2010, QDate(2010, 1, 1), QDate()),
    };
    const CalendarBuckets b = buildBuckets(movies, DateBasis::Release);

    QCOMPARE(b.day(QDate(2010, 1, 1)).size(), 1);   // only the child
    QCOMPARE(b.yearCount(2010), 1);
}

void TestCalendarBuckets::tracks_range_and_max_counts()
{
    const QList<Movie> movies = {
        mk(QStringLiteral("a"), 1985, QDate(1985, 6, 1), QDate()),
        mk(QStringLiteral("b"), 2003, QDate(2003, 6, 1), QDate()),
        mk(QStringLiteral("c"), 2003, QDate(2003, 6, 1), QDate()),
        mk(QStringLiteral("d"), 2003, QDate(2003, 8, 9), QDate()),
    };
    const CalendarBuckets b = buildBuckets(movies, DateBasis::Release);

    QCOMPARE(b.minYear, 1985);
    QCOMPARE(b.maxYear, 2003);
    QCOMPARE(b.maxDayCount, 2);    // 2003-06-01 has two
    QCOMPARE(b.maxMonthCount, 2);  // 2003-06 has two
    QCOMPARE(b.maxYearCount, 3);   // 2003 has three
}

QTEST_GUILESS_MAIN(TestCalendarBuckets)
#include "test_calendar_buckets.moc"
