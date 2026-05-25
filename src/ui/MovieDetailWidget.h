#pragma once

#include "domain/Movie.h"

#include <QFrame>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QScrollArea>

namespace xyz {

// ---------------------------------------------------------------------------
// MovieDetailWidget — scrollable detail pane for a single Movie.
// ---------------------------------------------------------------------------
// Fixed width 460px. Displays all fields of the selected movie grouped
// into collapsible sections (sections are hidden when their data is empty).
//
// Usage:
//   - Call updateFromMovie() when the selection changes.
//   - Call clearSelection() to show the "Select a movie" placeholder.
//   - Connect to tmdbSearchRequested() to launch the TMDb match dialog.
// ---------------------------------------------------------------------------
class MovieDetailWidget : public QScrollArea {
    Q_OBJECT

public:
    explicit MovieDetailWidget(QWidget* parent = nullptr);

    /// Populate every label / section from the given movie.
    void updateFromMovie(const Movie& movie);

    /// Reset to the empty-state placeholder ("Select a movie").
    void clearSelection();

Q_SIGNALS:
    /// Emitted when the user clicks the TMDb search/re-match button.
    void tmdbSearchRequested();

private:
    // --- Helpers -----------------------------------------------------------
    QLabel* makeHeader(const QString& text);
    QLabel* makeBody(const QString& text = {});

    // --- Inner widget & root layout ----------------------------------------
    QWidget*     m_inner      = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;

    // --- Placeholder -------------------------------------------------------
    QLabel* m_placeholder = nullptr;

    // --- Content container (hidden when no movie selected) ------------------
    QWidget* m_content = nullptr;

    // 1. Title block
    QLabel* m_titleLabel         = nullptr;
    QLabel* m_originalTitleLabel = nullptr;
    QLabel* m_distTraitLabel     = nullptr;
    QLabel* m_metaLineLabel      = nullptr;

    // 2. Cover image
    QLabel* m_coverLabel = nullptr;

    // 3. TMDb row
    QWidget*     m_tmdbRow    = nullptr;
    QLabel*      m_tmdbIdLabel = nullptr;
    QPushButton* m_tmdbButton  = nullptr;

    // 4. Loan badge
    QFrame* m_loanBadge      = nullptr;
    QLabel* m_loanTitleLabel  = nullptr;
    QLabel* m_loanDetailLabel = nullptr;

    // 5. Overview
    QWidget* m_overviewSection = nullptr;
    QLabel*  m_overviewHeader  = nullptr;
    QLabel*  m_overviewBody    = nullptr;

    // 6. Notes
    QWidget* m_notesSection = nullptr;
    QLabel*  m_notesHeader  = nullptr;
    QLabel*  m_notesBody    = nullptr;

    // 7. Easter Eggs
    QWidget* m_easterEggsSection = nullptr;
    QLabel*  m_easterEggsHeader  = nullptr;
    QLabel*  m_easterEggsBody    = nullptr;

    // 8. Genres
    QWidget* m_genresSection = nullptr;
    QLabel*  m_genresHeader  = nullptr;
    QLabel*  m_genresBody    = nullptr;

    // 9. Studios
    QWidget* m_studiosSection = nullptr;
    QLabel*  m_studiosHeader  = nullptr;
    QLabel*  m_studiosBody    = nullptr;

    // 10. Cast
    QWidget* m_castSection = nullptr;
    QLabel*  m_castHeader  = nullptr;
    QLabel*  m_castBody    = nullptr;

    // 11. Crew
    QWidget* m_crewSection = nullptr;
    QLabel*  m_crewHeader  = nullptr;
    QLabel*  m_crewBody    = nullptr;

    // 12. Audio
    QWidget* m_audioSection = nullptr;
    QLabel*  m_audioHeader  = nullptr;
    QLabel*  m_audioBody    = nullptr;

    // 13. Subtitles
    QWidget* m_subtitlesSection = nullptr;
    QLabel*  m_subtitlesHeader  = nullptr;
    QLabel*  m_subtitlesBody    = nullptr;

    // 14. Discs
    QWidget* m_discsSection = nullptr;
    QLabel*  m_discsHeader  = nullptr;
    QLabel*  m_discsBody    = nullptr;

    // Cover cache key
    QString m_cachedCoverPath;

    // 15. Technical
    QWidget* m_technicalSection = nullptr;
    QLabel*  m_technicalHeader  = nullptr;
    QLabel*  m_technicalBody    = nullptr;

    // 16. Purchase
    QWidget* m_purchaseSection = nullptr;
    QLabel*  m_purchaseHeader  = nullptr;
    QLabel*  m_purchaseBody    = nullptr;

    // 17. Tags
    QWidget* m_tagsSection = nullptr;
    QLabel*  m_tagsHeader  = nullptr;
    QLabel*  m_tagsBody    = nullptr;
};

} // namespace xyz
