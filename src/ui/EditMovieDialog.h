#pragma once

#include "domain/Movie.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QWidget;

namespace xyz {

class FlowLayout;

// Dialog for editing the most important properties of an existing movie.
// Constructed from a full Movie (a copy is kept), so unedited fields — cast,
// discs, box-set membership, tmdbId — are preserved; editedMovie() returns the
// copy with the form's values written back. Matches the AddTitleDialog look.
class EditMovieDialog : public QDialog {
    Q_OBJECT

public:
    explicit EditMovieDialog(const Movie& movie, QWidget* parent = nullptr);

    // The movie with the dialog's edits applied. Valid after exec() == Accepted.
    Movie editedMovie() const { return m_movie; }

private:
    void buildUi_();
    void addGenre_(const QString& genre);
    void rebuildGenreChips_();
    void commit_();   // write widgets back into m_movie, then accept()

    Movie       m_movie;
    QStringList m_genres;

    QLineEdit*      m_title       = nullptr;
    QLineEdit*      m_original    = nullptr;
    QLineEdit*      m_sortTitle   = nullptr;
    QSpinBox*       m_year        = nullptr;
    QSpinBox*       m_runtime     = nullptr;
    QComboBox*      m_format      = nullptr;
    QSpinBox*       m_rating      = nullptr;
    QLineEdit*      m_location    = nullptr;
    QDateEdit*      m_purchaseDate = nullptr;
    QWidget*        m_genreHost   = nullptr;
    FlowLayout*     m_genreFlow   = nullptr;
    QLineEdit*      m_genreInput  = nullptr;
    QPlainTextEdit* m_notes       = nullptr;
};

} // namespace xyz
