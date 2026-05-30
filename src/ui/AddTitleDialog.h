#pragma once

#include "tmdb/TmdbTypes.h"

#include <QDialog>
#include <QList>
#include <QStringList>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPushButton;
class QSpinBox;
class QToolButton;

namespace xyz {

class TmdbClient;

// "Add a title" dialog: a DVD-Profiler-style online lookup against TMDb with a
// rich set of search/filter criteria (title, year range, genres, person,
// minimum rating, original language, origin country, runtime range, result
// sorting). The caller reads selectedTmdbId() / selectedPosterPath() after the
// dialog closes Accepted and hands them to LibraryController::addMovieFromTmdb.
class AddTitleDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddTitleDialog(TmdbClient* tmdb, QWidget* parent = nullptr);

    int     selectedTmdbId() const { return m_selectedTmdbId; }
    QString selectedPosterPath() const { return m_selectedPosterPath; }

private:
    void buildUi_();
    void buildGenreMenu_();
    void runSearch_();
    void onResults_(const QList<TmdbCandidate>& candidates, const QString& error);
    QWidget* makeResultRow_(const TmdbCandidate& c);

    TmdbClient* m_tmdb = nullptr;

    QLineEdit*      m_titleEdit    = nullptr;
    QSpinBox*       m_yearFrom     = nullptr;
    QSpinBox*       m_yearTo       = nullptr;
    QToolButton*    m_genreBtn     = nullptr;
    QMenu*          m_genreMenu    = nullptr;
    QLineEdit*      m_personEdit   = nullptr;
    QDoubleSpinBox* m_minRating    = nullptr;
    QComboBox*      m_langCombo    = nullptr;
    QComboBox*      m_countryCombo = nullptr;
    QSpinBox*       m_runtimeMin   = nullptr;
    QSpinBox*       m_runtimeMax   = nullptr;
    QComboBox*      m_sortCombo    = nullptr;
    QPushButton*    m_searchBtn    = nullptr;

    QLabel*      m_statusLabel = nullptr;
    QListWidget* m_listWidget  = nullptr;
    QPushButton* m_addBtn      = nullptr;

    QList<TmdbCandidate> m_candidates;
    QList<int>           m_selectedGenreIds;
    QList<QLabel*>       m_posterLabels;
    QStringList          m_posterUrls;

    int     m_selectedTmdbId = 0;
    QString m_selectedPosterPath;
};

} // namespace xyz
