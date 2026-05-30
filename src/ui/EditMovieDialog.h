#pragma once

#include "domain/Movie.h"
#include "tmdb/TmdbTypes.h"

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace xyz {

class FlowLayout;
class TmdbClient;

// Dialog for adding or editing a movie. Constructed from a full Movie copy, so
// unedited fields — cast, discs, box-set membership — are preserved. TMDb is an
// optional prefill source; manual editing and saving works without it.
class EditMovieDialog : public QDialog {
    Q_OBJECT

public:
    explicit EditMovieDialog(const Movie& movie, QWidget* parent = nullptr);
    EditMovieDialog(const Movie& movie, TmdbClient* tmdb, bool isNew,
                    QWidget* parent = nullptr);

    // The movie with the dialog's edits applied. Valid after exec() == Accepted.
    Movie editedMovie() const { return m_movie; }
    QString tmdbPosterPath() const { return m_tmdbPosterPath; }
    QString selectedFrontCoverPath() const { return m_pendingFrontCoverPath; }
    QString selectedBackCoverPath() const { return m_pendingBackCoverPath; }

private:
    void buildUi_();
    void buildTmdbUi_(QVBoxLayout* root);
    void runTmdbSearch_();
    void onTmdbSearchFinished_(quint64 requestId,
                               const QList<TmdbCandidate>& candidates,
                               const QString& error);
    void onTmdbMovieFinished_(const TmdbMovieDetails& details,
                              const QString& error);
    void applyTmdbDetails_(const TmdbMovieDetails& details);
    QWidget* makeTmdbResultRow_(const TmdbCandidate& candidate);
    QWidget* makeCoverPanel_(const QString& title, QLabel* preview,
                             QPushButton** chooseButton, QPushButton** clearButton);
    void addGenre_(const QString& genre);
    void rebuildGenreChips_();
    void updateCoverPreview_(QLabel* label, const QString& path);
    void loadTmdbPosterPreview_(const QString& posterPath);
    void pickCover_(bool front);
    void commit_();   // write widgets back into m_movie, then accept()

    Movie       m_movie;
    TmdbClient* m_tmdb = nullptr;
    bool        m_isNew = false;
    QStringList m_genres;
    QList<TmdbCandidate> m_tmdbCandidates;
    quint64     m_tmdbRequestId = 0;
    int         m_pendingTmdbId = 0;
    QString     m_tmdbPosterPath;
    QString     m_pendingFrontCoverPath;
    QString     m_pendingBackCoverPath;

    QLineEdit*      m_title       = nullptr;
    QLineEdit*      m_original    = nullptr;
    QLineEdit*      m_sortTitle   = nullptr;
    QLineEdit*      m_distTrait   = nullptr;
    QSpinBox*       m_year        = nullptr;
    QSpinBox*       m_runtime     = nullptr;
    QComboBox*      m_format      = nullptr;
    QLineEdit*      m_ratingSystem = nullptr;
    QLineEdit*      m_ratingValue = nullptr;
    QSpinBox*       m_ratingAge   = nullptr;
    QSpinBox*       m_rating      = nullptr;
    QLineEdit*      m_studios     = nullptr;
    QLineEdit*      m_countries   = nullptr;
    QLineEdit*      m_mediaCompanies = nullptr;
    QLineEdit*      m_location    = nullptr;
    QDateEdit*      m_purchaseDate = nullptr;
    QLineEdit*      m_purchasePlace = nullptr;
    QLabel*         m_coverFrontPreview = nullptr;
    QLabel*         m_coverBackPreview = nullptr;
    QPushButton*    m_chooseFrontCover = nullptr;
    QPushButton*    m_chooseBackCover = nullptr;
    QPushButton*    m_clearFrontCover = nullptr;
    QPushButton*    m_clearBackCover = nullptr;
    QWidget*        m_genreHost   = nullptr;
    FlowLayout*     m_genreFlow   = nullptr;
    QLineEdit*      m_genreInput  = nullptr;
    QPlainTextEdit* m_overview    = nullptr;
    QPlainTextEdit* m_notes       = nullptr;

    QLineEdit*      m_tmdbSearch  = nullptr;
    QListWidget*    m_tmdbResults = nullptr;
    QPushButton*    m_tmdbApply   = nullptr;
    QLabel*         m_tmdbStatus  = nullptr;
};

} // namespace xyz
