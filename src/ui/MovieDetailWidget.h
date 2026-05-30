#pragma once

#include "domain/Movie.h"

#include <QWidget>

class QFrame;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QVBoxLayout;

namespace xyz {

class StarBar;

// ---------------------------------------------------------------------------
// MovieDetailWidget — the detail pane (ported from the Claude Design layout).
// ---------------------------------------------------------------------------
// Fixed-width pane with a header (cover, title, meta line incl. FSK badge,
// genre chips, "my rating", actions), an optional loan banner, and a tab
// strip (Overview / Cast & Crew / Tech / Notes). Sections without data are
// hidden. Bind via updateFromMovie(); show the placeholder via
// clearSelection(); re-skin after a theme switch via refreshTheme().
// ---------------------------------------------------------------------------
class MovieDetailWidget : public QWidget {
    Q_OBJECT

public:
    explicit MovieDetailWidget(QWidget* parent = nullptr);

    void updateFromMovie(const Movie& movie);
    void clearSelection();
    void refreshTheme();

Q_SIGNALS:
    void tmdbSearchRequested();

private:
    void buildUi_();
    void buildHeader_(QVBoxLayout* contentLayout);
    void buildTabs_(QVBoxLayout* contentLayout);

    void populateHeader_(const Movie& m);
    void populateOverview_(const Movie& m);
    void populateCast_(const Movie& m);
    void populateTech_(const Movie& m);
    void populateNotes_(const Movie& m);

    // Tab indices, in the order buildTabs_ adds them.
    enum Tab { TabOverview = 0, TabCast, TabTech, TabNotes, TabCount };
    // Populate one tab from m_current if it hasn't been populated for the
    // current movie yet. updateFromMovie only fills the visible tab; the rest
    // are filled on demand when the user switches to them. This keeps each
    // selection change cheap (header + one tab) instead of rebuilding all four.
    void populateTab_(int index);

    // --- Top-level ----------------------------------------------------------
    QStackedWidget* m_stack       = nullptr;   // 0 = placeholder, 1 = content
    bool            m_hasMovie    = false;
    Movie           m_current;
    bool            m_tabPopulated[TabCount] = {false, false, false, false};

    // --- Header -------------------------------------------------------------
    QLabel*      m_cover     = nullptr;
    QLabel*      m_title     = nullptr;
    QLabel*      m_original  = nullptr;
    QWidget*     m_metaRow   = nullptr;
    QHBoxLayout* m_metaLayout = nullptr;
    QWidget*     m_chips     = nullptr;         // FlowLayout host
    QWidget*     m_ratingRow = nullptr;
    StarBar*     m_stars     = nullptr;
    QLabel*      m_ratingCap = nullptr;
    QPushButton* m_tmdbBtn   = nullptr;

    // --- Loan banner --------------------------------------------------------
    QFrame*  m_loanBanner = nullptr;
    QLabel*  m_loanIcon   = nullptr;
    QLabel*  m_loanText   = nullptr;

    // --- Tabs ---------------------------------------------------------------
    QTabWidget* m_tabs = nullptr;

    // Overview tab
    QLabel*      m_overviewText = nullptr;
    QGridLayout* m_factsGrid    = nullptr;
    QWidget*     m_bonusSection = nullptr;
    QVBoxLayout* m_bonusLayout  = nullptr;

    // Cast & Crew tab
    QVBoxLayout* m_castLayout = nullptr;
    QWidget*     m_castSection = nullptr;
    QVBoxLayout* m_crewLayout = nullptr;
    QWidget*     m_crewSection = nullptr;

    // Tech tab
    QGridLayout* m_specGrid = nullptr;

    // Notes tab
    QLabel*      m_notesText  = nullptr;
    QGridLayout* m_notesGrid  = nullptr;
};

} // namespace xyz
