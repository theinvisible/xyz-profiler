// Unit tests for the pure domain helpers that the Block A UI is built on:
// collection-status matching, monetary display, and the lend/return
// mutations. All header-only and GUI-free, so QTEST_GUILESS_MAIN applies.

#include "domain/CollectionMembership.h"
#include "domain/LoanOps.h"
#include "domain/MonetaryAmount.h"
#include "domain/Movie.h"

#include <QTest>

using namespace xyz;

namespace {

CollectionMembership membership(const QString& type, bool partOfOwned = true)
{
    CollectionMembership m;
    m.type = type;
    m.isPartOfOwnedCollection = partOfOwned;
    return m;
}

} // namespace

class TestDomainOps : public QObject {
    Q_OBJECT

private slots:
    // --- CollectionMembership ---------------------------------------------
    void wishlistDetection_data();
    void wishlistDetection();
    void statusFiltersArePartition();
    void statusKeysRoundTrip();
    void unknownStatusKeyFallsBackToOwned();

    // --- MonetaryAmount ----------------------------------------------------
    void displayAmountPrefersSourceFormatting();
    void displayAmountFallsBackToValueAndCurrency();
    void displayAmountEmptyWhenNoValue();

    // --- LoanOps -----------------------------------------------------------
    void lendSetsLoanAndAppendsEvent();
    void returnClearsLoanAndAppendsEvent();
    void returnIsNoOpWhenNotLoaned();
    void historyIsPreservedAcrossLendCycles();
};

// ---------------------------------------------------------------------------
void TestDomainOps::wishlistDetection_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<bool>("partOfOwned");
    QTest::addColumn<bool>("expectedWishlist");

    QTest::newRow("owned")            << QStringLiteral("Owned")    << true  << false;
    QTest::newRow("wishlist")         << QStringLiteral("Wishlist") << false << true;
    // DP4 casing is not guaranteed across export versions.
    QTest::newRow("wishlist lower")   << QStringLiteral("wishlist") << false << true;
    QTest::newRow("wishlist upper")   << QStringLiteral("WISHLIST") << false << true;
    // Other DP4 types count as owned so the two filters stay exhaustive.
    QTest::newRow("order")            << QStringLiteral("Order")    << false << false;
    QTest::newRow("for sale")         << QStringLiteral("For Sale") << true  << false;
    // No type at all: fall back to the IsPartOfOwnedCollection attribute.
    QTest::newRow("empty, owned")     << QString()                  << true  << false;
    QTest::newRow("empty, not owned") << QString()                  << false << true;
}

void TestDomainOps::wishlistDetection()
{
    QFETCH(QString, type);
    QFETCH(bool, partOfOwned);
    QFETCH(bool, expectedWishlist);

    QCOMPARE(isWishlistMembership(membership(type, partOfOwned)), expectedWishlist);
}

void TestDomainOps::statusFiltersArePartition()
{
    // Every title must be visible under exactly one of Owned / Wishlist, and
    // under All regardless — otherwise a filter could make entries vanish.
    const QList<CollectionMembership> all = {
        membership(QStringLiteral("Owned")),
        membership(QStringLiteral("Wishlist"), false),
        membership(QStringLiteral("Order"), false),
        membership(QString(), true),
        membership(QString(), false),
    };

    for (const auto& m : all) {
        const bool owned    = matchesStatus(m, CollectionStatus::Owned);
        const bool wishlist = matchesStatus(m, CollectionStatus::Wishlist);
        QVERIFY2(owned != wishlist, qPrintable(QStringLiteral("type=%1").arg(m.type)));
        QVERIFY(matchesStatus(m, CollectionStatus::All));
    }
}

void TestDomainOps::statusKeysRoundTrip()
{
    for (auto status : {CollectionStatus::Owned, CollectionStatus::Wishlist,
                        CollectionStatus::All}) {
        QCOMPARE(collectionStatusFromKey(collectionStatusKey(status)), status);
    }
}

void TestDomainOps::unknownStatusKeyFallsBackToOwned()
{
    // A hand-edited INI must not put the views into an undefined state.
    QCOMPARE(collectionStatusFromKey(QStringLiteral("nonsense")),
             CollectionStatus::Owned);
    QCOMPARE(collectionStatusFromKey(QString()), CollectionStatus::Owned);
}

// ---------------------------------------------------------------------------
void TestDomainOps::displayAmountPrefersSourceFormatting()
{
    MonetaryAmount a;
    a.value = QStringLiteral("14.99");
    a.denominationType = QStringLiteral("EUR");
    a.formattedValue = QStringLiteral("14,99 €");
    QCOMPARE(displayAmount(a), QStringLiteral("14,99 €"));
}

void TestDomainOps::displayAmountFallsBackToValueAndCurrency()
{
    MonetaryAmount a;
    a.value = QStringLiteral("14.99");
    a.denominationType = QStringLiteral("EUR");
    QCOMPARE(displayAmount(a), QStringLiteral("14.99 EUR"));

    a.denominationType.clear();
    QCOMPARE(displayAmount(a), QStringLiteral("14.99"));
}

void TestDomainOps::displayAmountEmptyWhenNoValue()
{
    MonetaryAmount a;
    QVERIFY(displayAmount(a).isEmpty());

    // A currency without an amount is not a price.
    a.denominationType = QStringLiteral("EUR");
    QVERIFY(displayAmount(a).isEmpty());
}

// ---------------------------------------------------------------------------
void TestDomainOps::lendSetsLoanAndAppendsEvent()
{
    Movie m;
    const QDate due(2026, 8, 14);
    const QDateTime when(QDate(2026, 7, 26), QTime(18, 30));

    lendItem(m, QStringLiteral("Markus"), QStringLiteral("Bauer"), due, when);

    QVERIFY(m.loan.loaned);
    QCOMPARE(m.loan.due, due);
    QCOMPARE(m.loan.userFirstName, QStringLiteral("Markus"));
    QCOMPARE(m.loan.userLastName, QStringLiteral("Bauer"));

    QCOMPARE(m.events.size(), 1);
    QCOMPARE(m.events.first().type, loanEventBorrowed());
    QCOMPARE(m.events.first().timestamp, when);
    QCOMPARE(m.events.first().userFirstName, QStringLiteral("Markus"));
}

void TestDomainOps::returnClearsLoanAndAppendsEvent()
{
    Movie m;
    const QDateTime lentAt(QDate(2026, 7, 26), QTime(18, 30));
    const QDateTime backAt(QDate(2026, 8, 2), QTime(9, 0));

    lendItem(m, QStringLiteral("Anna"), QStringLiteral("Klein"),
             QDate(2026, 8, 14), lentAt);
    returnItem(m, backAt);

    QVERIFY(!m.loan.loaned);
    QVERIFY(!m.loan.due.isValid());
    QVERIFY(m.loan.userFirstName.isEmpty());

    QCOMPARE(m.events.size(), 2);
    QCOMPARE(m.events.last().type, loanEventReturned());
    QCOMPARE(m.events.last().timestamp, backAt);
    // The return event keeps the borrower so the history stays readable.
    QCOMPARE(m.events.last().userFirstName, QStringLiteral("Anna"));
}

void TestDomainOps::returnIsNoOpWhenNotLoaned()
{
    Movie m;
    returnItem(m, QDateTime(QDate(2026, 8, 2), QTime(9, 0)));
    QVERIFY(m.events.isEmpty());
    QVERIFY(!m.loan.loaned);
}

void TestDomainOps::historyIsPreservedAcrossLendCycles()
{
    // Imported DP4 history must survive local lending.
    Movie m;
    Event imported;
    imported.type = loanEventBorrowed();
    imported.timestamp = QDateTime(QDate(2019, 3, 1), QTime(12, 0));
    imported.userFirstName = QStringLiteral("Legacy");
    m.events << imported;

    lendItem(m, QStringLiteral("Markus"), QStringLiteral("Bauer"), QDate(),
             QDateTime(QDate(2026, 7, 26), QTime(18, 30)));
    returnItem(m, QDateTime(QDate(2026, 8, 2), QTime(9, 0)));

    QCOMPARE(m.events.size(), 3);
    QCOMPARE(m.events.first().userFirstName, QStringLiteral("Legacy"));
}

QTEST_GUILESS_MAIN(TestDomainOps)
#include "test_domain_ops.moc"
