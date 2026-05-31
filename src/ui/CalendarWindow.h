#pragma once

#include "ui/CalendarBuckets.h"   // DateBasis, CalendarBuckets, xyz::Movie
#include "ui/CalendarPaint.h"     // CoverIndex

#include <QDate>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QToolButton;

namespace xyz {

class LibraryController;
class SettingsController;
class TmdbClient;
class CalendarMonthView;
class CalendarYearView;
class CalendarTimeline;
class MoviePopover;
class DayPopover;

// Standalone, non-modal window that visualises the collection over time. The
// user picks a date basis (release / purchase), switches month/year views,
// navigates, and clicks the timeline to jump. Clicking a film opens its detail
// in an anchored popover. Owned (kept alive) by MainWindow.
class CalendarWindow : public QWidget {
    Q_OBJECT

public:
    CalendarWindow(LibraryController* controller, SettingsController* settings,
                   TmdbClient* tmdb, QWidget* parent = nullptr);

    void refreshTheme();

private:
    void buildUi_();
    void rebuild_();          // recompute buckets + cover index from the movie list
    void updateViews_();      // push anchor/view to the widgets + labels
    void setBasis_(DateBasis basis);
    void setYearView_(bool yearView);
    void stepPeriod_(int dir);
    void goToday_();
    void showMoviePopover_(const QString& id);
    void showDayPopover_(const QDate& date);
    void retintIcons_();
    const Movie* findMovie_(const QString& id) const;

    LibraryController*  m_controller = nullptr;
    SettingsController* m_settings   = nullptr;
    TmdbClient*         m_tmdb       = nullptr;

    DateBasis m_basis    = DateBasis::Release;
    bool      m_yearView = false;
    QDate     m_anchor;
    bool      m_snapped  = false;   // one-time snap of the anchor onto the data range

    CalendarBuckets m_buckets;
    CoverIndex      m_coverIndex;

    // Header controls
    QToolButton* m_basisRelease  = nullptr;
    QToolButton* m_basisPurchase = nullptr;
    QToolButton* m_viewMonth     = nullptr;
    QToolButton* m_viewYear      = nullptr;
    QToolButton* m_prevBtn       = nullptr;
    QToolButton* m_nextBtn       = nullptr;
    QToolButton* m_todayBtn      = nullptr;
    QLabel*      m_titleLabel    = nullptr;
    QLabel*      m_periodLabel   = nullptr;
    QLabel*      m_infoLabel     = nullptr;

    // Views
    QStackedWidget*    m_stack     = nullptr;
    CalendarMonthView* m_monthView = nullptr;
    CalendarYearView*  m_yearView_ = nullptr;
    CalendarTimeline*  m_timeline  = nullptr;

    MoviePopover* m_popover    = nullptr;
    DayPopover*   m_dayPopover = nullptr;
};

} // namespace xyz
