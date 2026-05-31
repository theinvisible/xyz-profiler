#include "ui/CalendarWindow.h"

#include "controllers/LibraryController.h"
#include "controllers/SettingsController.h"
#include "models/MovieListModel.h"
#include "ui/CalendarMonthView.h"
#include "ui/CalendarTimeline.h"
#include "ui/CalendarYearView.h"
#include "ui/CoverCache.h"
#include "ui/DayPopover.h"
#include "ui/IconFactory.h"
#include "ui/MoviePopover.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCursor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace xyz {
namespace {

// A segmented toggle button styled like the main window's view switch.
QToolButton* makeSegButton(const QString& text, QButtonGroup* group, QWidget* seg)
{
    auto* b = new QToolButton(seg);
    b->setObjectName(QStringLiteral("segText"));
    b->setText(text);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setToolButtonStyle(Qt::ToolButtonTextOnly);
    seg->layout()->addWidget(b);
    group->addButton(b);
    return b;
}

QWidget* makeSeg(QWidget* parent)
{
    auto* seg = new QWidget(parent);
    seg->setObjectName(QStringLiteral("viewSeg"));
    auto* l = new QHBoxLayout(seg);
    l->setContentsMargins(3, 3, 3, 3);
    l->setSpacing(2);
    return seg;
}

} // namespace

CalendarWindow::CalendarWindow(LibraryController* controller,
                               SettingsController* settings,
                               TmdbClient* tmdb, QWidget* parent)
    : QWidget(parent, Qt::Window),
      m_controller(controller),
      m_settings(settings),
      m_tmdb(tmdb),
      m_anchor(QDate::currentDate())
{
    m_basis = (m_settings->calendarDateBasis() == QLatin1String("purchase"))
                  ? DateBasis::Purchase : DateBasis::Release;
    m_yearView = (m_settings->calendarView() == QLatin1String("year"));

    setObjectName(QStringLiteral("calendarWindow"));
    setWindowTitle(tr("Calendar"));
    resize(1040, 720);
    setMinimumSize(760, 540);

    buildUi_();

    connect(m_controller, &LibraryController::moviesChanged,
            this, &CalendarWindow::rebuild_);
    connect(m_controller, &LibraryController::coverUpdated, this,
            [this](const QString& path) {
        CoverCache::bump(path);
        m_monthView->update();
        m_yearView_->update();
    });

    refreshTheme();
    rebuild_();
}

void CalendarWindow::buildUi_()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    // ---- Header --------------------------------------------------------
    auto* header = new QHBoxLayout;
    header->setSpacing(8);

    m_titleLabel = new QLabel(tr("Calendar"), this);
    QFont tf = m_titleLabel->font();
    tf.setPointSizeF(tf.pointSizeF() + 3.0);
    tf.setBold(true);
    m_titleLabel->setFont(tf);
    header->addWidget(m_titleLabel);
    header->addSpacing(14);

    // Date-basis toggle (the first thing the user picks).
    auto* basisSeg = makeSeg(this);
    auto* basisGroup = new QButtonGroup(this);
    basisGroup->setExclusive(true);
    m_basisRelease  = makeSegButton(tr("Release date"),  basisGroup, basisSeg);
    m_basisPurchase = makeSegButton(tr("Purchase date"), basisGroup, basisSeg);
    header->addWidget(basisSeg);

    header->addStretch(1);

    // Month / Year toggle.
    auto* viewSeg = makeSeg(this);
    auto* viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    m_viewMonth = makeSegButton(tr("Month"), viewGroup, viewSeg);
    m_viewYear  = makeSegButton(tr("Year"),  viewGroup, viewSeg);
    header->addWidget(viewSeg);
    header->addSpacing(10);

    // Navigation.
    m_prevBtn = new QToolButton(this);
    m_prevBtn->setObjectName(QStringLiteral("tbIcon"));
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setToolTip(tr("Previous"));
    header->addWidget(m_prevBtn);

    m_periodLabel = new QLabel(this);
    m_periodLabel->setAlignment(Qt::AlignCenter);
    m_periodLabel->setMinimumWidth(150);
    QFont pf = m_periodLabel->font();
    pf.setBold(true);
    m_periodLabel->setFont(pf);
    header->addWidget(m_periodLabel);

    m_nextBtn = new QToolButton(this);
    m_nextBtn->setObjectName(QStringLiteral("tbIcon"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setToolTip(tr("Next"));
    header->addWidget(m_nextBtn);

    header->addSpacing(8);
    m_todayBtn = new QToolButton(this);
    m_todayBtn->setText(tr("Today"));
    m_todayBtn->setCursor(Qt::PointingHandCursor);
    m_todayBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    header->addWidget(m_todayBtn);

    root->addLayout(header);

    // ---- Views ---------------------------------------------------------
    m_stack = new QStackedWidget(this);
    m_monthView = new CalendarMonthView(this);
    m_yearView_ = new CalendarYearView(this);
    m_stack->addWidget(m_monthView);   // index 0
    m_stack->addWidget(m_yearView_);   // index 1
    root->addWidget(m_stack, 1);

    // ---- Info + timeline ----------------------------------------------
    m_infoLabel = new QLabel(this);
    m_infoLabel->setVisible(false);
    root->addWidget(m_infoLabel);

    m_timeline = new CalendarTimeline(this);
    root->addWidget(m_timeline);

    // ---- Initial checked states ---------------------------------------
    (m_basis == DateBasis::Purchase ? m_basisPurchase : m_basisRelease)->setChecked(true);
    (m_yearView ? m_viewYear : m_viewMonth)->setChecked(true);
    m_stack->setCurrentIndex(m_yearView ? 1 : 0);

    // ---- Wiring --------------------------------------------------------
    connect(m_basisRelease,  &QToolButton::clicked, this,
            [this] { setBasis_(DateBasis::Release); });
    connect(m_basisPurchase, &QToolButton::clicked, this,
            [this] { setBasis_(DateBasis::Purchase); });
    connect(m_viewMonth, &QToolButton::clicked, this, [this] { setYearView_(false); });
    connect(m_viewYear,  &QToolButton::clicked, this, [this] { setYearView_(true); });
    connect(m_prevBtn,  &QToolButton::clicked, this, [this] { stepPeriod_(-1); });
    connect(m_nextBtn,  &QToolButton::clicked, this, [this] { stepPeriod_(+1); });
    connect(m_todayBtn, &QToolButton::clicked, this, &CalendarWindow::goToday_);

    connect(m_monthView, &CalendarMonthView::movieActivated,
            this, &CalendarWindow::showMoviePopover_);
    connect(m_monthView, &CalendarMonthView::dayActivated,
            this, &CalendarWindow::showDayPopover_);
    connect(m_yearView_, &CalendarYearView::monthActivated, this,
            [this](int year, int month) {
        m_anchor = QDate(year, month, 1);
        setYearView_(false);
    });
    connect(m_timeline, &CalendarTimeline::yearActivated, this, [this](int year) {
        m_anchor = QDate(year, m_anchor.month(), 1);
        updateViews_();
    });
}

void CalendarWindow::rebuild_()
{
    const QList<Movie>& movies = m_controller->listModel()->movies();
    m_buckets = buildBuckets(movies, m_basis);

    m_coverIndex.clear();
    m_coverIndex.reserve(movies.size());
    for (const Movie& m : movies) {
        if (m.boxSet.isParent) continue;
        m_coverIndex.insert(m.id, CoverRef{m.coverFrontPath, m.title, m.productionYear});
    }

    // On first populated rebuild, snap the anchor onto the data if "today" lies
    // outside the collection's range, so the window opens on real data.
    if (!m_snapped && m_buckets.minYear != 0) {
        if (m_anchor.year() < m_buckets.minYear || m_anchor.year() > m_buckets.maxYear)
            m_anchor = QDate(m_buckets.maxYear, qBound(1, m_anchor.month(), 12), 1);
        m_snapped = true;
    }

    m_monthView->setData(&m_buckets, &m_coverIndex);
    m_yearView_->setData(&m_buckets, &m_coverIndex);
    m_timeline->setData(&m_buckets);
    updateViews_();
}

void CalendarWindow::updateViews_()
{
    m_monthView->setMonth(m_anchor.year(), m_anchor.month());
    m_yearView_->setYear(m_anchor.year());
    m_timeline->setCurrentYear(m_anchor.year());

    const QLocale loc;
    m_periodLabel->setText(m_yearView
        ? QString::number(m_anchor.year())
        : QStringLiteral("%1 %2").arg(
              loc.standaloneMonthName(m_anchor.month(), QLocale::LongFormat),
              QString::number(m_anchor.year())));

    QStringList parts;
    if (m_basis == DateBasis::Release && m_buckets.yearOnlyCount > 0)
        parts << tr("%n with year only", nullptr, m_buckets.yearOnlyCount);
    if (m_buckets.undatedCount > 0)
        parts << tr("%n without a date", nullptr, m_buckets.undatedCount);
    m_infoLabel->setText(parts.join(QStringLiteral("    ·    ")));
    m_infoLabel->setVisible(!parts.isEmpty());
}

void CalendarWindow::setBasis_(DateBasis basis)
{
    if (m_basis == basis) return;
    m_basis = basis;
    m_settings->setCalendarDateBasis(basis == DateBasis::Purchase
                                         ? QStringLiteral("purchase")
                                         : QStringLiteral("release"));
    rebuild_();
}

void CalendarWindow::setYearView_(bool yearView)
{
    m_yearView = yearView;
    m_settings->setCalendarView(yearView ? QStringLiteral("year")
                                         : QStringLiteral("month"));
    m_stack->setCurrentIndex(yearView ? 1 : 0);
    (yearView ? m_viewYear : m_viewMonth)->setChecked(true);
    updateViews_();
}

void CalendarWindow::stepPeriod_(int dir)
{
    m_anchor = m_yearView ? m_anchor.addYears(dir) : m_anchor.addMonths(dir);
    updateViews_();
}

void CalendarWindow::goToday_()
{
    m_anchor = QDate::currentDate();
    updateViews_();
}

const Movie* CalendarWindow::findMovie_(const QString& id) const
{
    return m_controller->listModel()->find(id);
}

void CalendarWindow::showMoviePopover_(const QString& id)
{
    const Movie* m = findMovie_(id);
    if (!m) return;
    m_controller->selectMovie(id);
    if (m_dayPopover && m_dayPopover->isVisible())
        m_dayPopover->hide();
    if (!m_popover) {
        m_popover = new MoviePopover(this);
        connect(m_popover, &MoviePopover::tmdbSearchRequested,
                m_controller, &LibraryController::searchSelectedOnTmdb);
    }
    m_popover->showFor(*m, QCursor::pos());
}

void CalendarWindow::showDayPopover_(const QDate& date)
{
    if (!m_dayPopover) {
        m_dayPopover = new DayPopover(this);
        connect(m_dayPopover, &DayPopover::movieActivated,
                this, &CalendarWindow::showMoviePopover_);
    }
    const qreal dpr = devicePixelRatioF();
    QList<DayPopover::Entry> entries;
    for (const QString& id : m_buckets.day(date)) {
        const Movie* m = findMovie_(id);
        DayPopover::Entry e;
        e.id    = id;
        e.title = m ? m->title : id;
        e.year  = m ? m->productionYear : 0;
        e.icon  = QIcon(calendarMiniCover(m_coverIndex.value(id), QSize(28, 42), dpr));
        entries.append(e);
    }
    const QLocale loc;
    m_dayPopover->showFor(loc.toString(date, QLocale::LongFormat), entries, QCursor::pos());
}

void CalendarWindow::retintIcons_()
{
    const Palette& pal = Theme::current();
    m_prevBtn->setIcon(IconFactory::icon(QStringLiteral("chevronLeft"), pal.text2, 18));
    m_nextBtn->setIcon(IconFactory::icon(QStringLiteral("chevron"), pal.text2, 18));
}

void CalendarWindow::refreshTheme()
{
    const Palette& pal = Theme::current();
    setStyleSheet(QStringLiteral("#calendarWindow { background:%1; }").arg(pal.win.name()));
    m_titleLabel->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text.name()));
    m_periodLabel->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text.name()));
    m_infoLabel->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text3.name()));
    retintIcons_();
    m_monthView->update();
    m_yearView_->update();
    m_timeline->update();
    if (m_popover)    m_popover->refreshTheme();
    if (m_dayPopover) m_dayPopover->refreshTheme();
}

} // namespace xyz
