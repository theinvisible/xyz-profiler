// Integration test for CollectionFilterProxyModel over the real
// MovieListModel: the owned / wishlist / all filter plus the grid's
// "hide box-set parents" criterion that it took over from the previous
// regex-based proxy.

#include "models/CollectionFilterProxyModel.h"
#include "models/MovieListModel.h"

#include <QTest>

using namespace xyz;

namespace {

Movie make(const QString& id, const QString& status, bool boxSetParent = false)
{
    Movie m;
    m.id = id;
    m.title = id;
    m.membership.type = status;
    m.membership.isPartOfOwnedCollection =
        (status.compare(QLatin1String("Wishlist"), Qt::CaseInsensitive) != 0);
    m.boxSet.isParent = boxSetParent;
    return m;
}

QStringList idsOf(const QAbstractItemModel& model)
{
    QStringList ids;
    for (int r = 0; r < model.rowCount(); ++r)
        ids << model.index(r, 0).data(MovieListModel::IdRole).toString();
    return ids;
}

} // namespace

class TestCollectionFilter : public QObject {
    Q_OBJECT

private slots:
    void init();
    void defaultsToOwned();
    void wishlistShowsOnlyWishlist();
    void allShowsEverything();
    void hidesBoxSetParentsWhenAsked();
    void boxSetParentFilterIsIndependentOfStatus();
    void withoutRolesEverythingPasses();
    void statusChangeInvalidatesTheFilter();

private:
    MovieListModel m_model;
    CollectionFilterProxyModel m_proxy;
};

void TestCollectionFilter::init()
{
    m_model.setMovies({
        make(QStringLiteral("owned1"),  QStringLiteral("Owned")),
        make(QStringLiteral("wish1"),   QStringLiteral("Wishlist")),
        make(QStringLiteral("owned2"),  QStringLiteral("Owned"), /*parent*/ true),
        make(QStringLiteral("wish2"),   QStringLiteral("wishlist")),   // lower case
        make(QStringLiteral("order1"),  QStringLiteral("Order")),      // counts as owned
    });
    m_proxy.setSourceModel(&m_model);
    m_proxy.setRoles(MovieListModel::MembershipTypeRole,
                     MovieListModel::MembershipIsOwnedRole,
                     MovieListModel::IsBoxSetParentRole);
    m_proxy.setHideBoxSetParents(false);
    m_proxy.setStatus(CollectionStatus::Owned);
}

void TestCollectionFilter::defaultsToOwned()
{
    CollectionFilterProxyModel fresh;
    QCOMPARE(fresh.status(), CollectionStatus::Owned);
}

void TestCollectionFilter::wishlistShowsOnlyWishlist()
{
    m_proxy.setStatus(CollectionStatus::Wishlist);
    QCOMPARE(idsOf(m_proxy),
             (QStringList{QStringLiteral("wish1"), QStringLiteral("wish2")}));
}

void TestCollectionFilter::allShowsEverything()
{
    m_proxy.setStatus(CollectionStatus::All);
    QCOMPARE(m_proxy.rowCount(), 5);
}

void TestCollectionFilter::hidesBoxSetParentsWhenAsked()
{
    m_proxy.setStatus(CollectionStatus::All);
    m_proxy.setHideBoxSetParents(true);
    QVERIFY(!idsOf(m_proxy).contains(QStringLiteral("owned2")));
    QCOMPARE(m_proxy.rowCount(), 4);
}

void TestCollectionFilter::boxSetParentFilterIsIndependentOfStatus()
{
    // The grid hides parents whatever the status filter says.
    m_proxy.setHideBoxSetParents(true);
    m_proxy.setStatus(CollectionStatus::Owned);
    QCOMPARE(idsOf(m_proxy),
             (QStringList{QStringLiteral("owned1"), QStringLiteral("order1")}));
}

void TestCollectionFilter::withoutRolesEverythingPasses()
{
    // A consumer that never calls setRoles() must not silently show nothing.
    CollectionFilterProxyModel bare;
    bare.setSourceModel(&m_model);
    bare.setStatus(CollectionStatus::Wishlist);
    QCOMPARE(bare.rowCount(), m_model.rowCount());
}

void TestCollectionFilter::statusChangeInvalidatesTheFilter()
{
    QCOMPARE(idsOf(m_proxy),
             (QStringList{QStringLiteral("owned1"), QStringLiteral("owned2"),
                          QStringLiteral("order1")}));
    m_proxy.setStatus(CollectionStatus::Wishlist);
    QCOMPARE(m_proxy.rowCount(), 2);
    m_proxy.setStatus(CollectionStatus::Owned);
    QCOMPARE(m_proxy.rowCount(), 3);
}

QTEST_GUILESS_MAIN(TestCollectionFilter)
#include "test_collection_filter.moc"
