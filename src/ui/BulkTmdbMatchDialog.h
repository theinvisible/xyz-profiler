#pragma once

#include "domain/Movie.h"
#include "tmdb/TmdbTypes.h"

#include <QDialog>
#include <QHash>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;

namespace xyz {

class TmdbClient;

// Bulk "Match on TMDb" dialog. Given a set of movies, it searches TMDb for each
// (bounded-concurrent, live), shows one row per movie with a best-guess
// candidate preselected and a combo to override or skip, plus a poster preview.
// Nothing is applied until the user clicks Assign; the caller then reads
// matches() / downloadPosters() and hands them to
// LibraryController::applyTmdbMatches.
class BulkTmdbMatchDialog : public QDialog {
    Q_OBJECT

public:
    BulkTmdbMatchDialog(TmdbClient* tmdb, const QList<Movie>& movies,
                        QWidget* parent = nullptr);

    QList<TmdbBulkMatch> matches() const;
    bool                 downloadPosters() const;

private:
    void buildUi_();
    void startSearches_();
    void pumpSearchQueue_();                 // keep kMaxInFlight searches running
    void onSearchResult_(quint64 requestId,
                         const QList<TmdbCandidate>& candidates,
                         const QString& error);
    void populateRow_(int row, const QList<TmdbCandidate>& candidates);
    void loadPosterForRow_(int row);
    void updateProgress_();

    TmdbClient*   m_tmdb = nullptr;
    QList<Movie>  m_movies;

    QTableWidget* m_table       = nullptr;
    QProgressBar* m_progress    = nullptr;
    QLabel*       m_progressLbl = nullptr;
    QCheckBox*    m_posterCheck = nullptr;
    QPushButton*  m_assignBtn   = nullptr;

    // Per-row candidate lists, indexed by row.
    QHash<int, QList<TmdbCandidate>> m_rowCandidates;

    int m_nextToStart = 0;   // next row index to dispatch a search for
    int m_inFlight    = 0;
    int m_completed   = 0;
};

} // namespace xyz
