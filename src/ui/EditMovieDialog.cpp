#include "ui/EditMovieDialog.h"

#include "ui/FlowLayout.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

#include <functional>

namespace xyz {
namespace {

// Sentinel date meaning "no purchase date" — QDateEdit can't be empty, so the
// minimum date doubles as the unset state and shows specialValueText.
const QDate kNoDate(1900, 1, 1);

QLabel* fieldLabel(const QString& text)
{
    auto* l = new QLabel(text);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text2.name()));
    return l;
}

// One removable genre chip: pill label + "×" button, styled like the detail
// pane's genre chips (MovieDetailWidget::chip).
QWidget* makeChip(const QString& text, QWidget* parent,
                  const std::function<void()>& onRemove)
{
    const Palette& p = Theme::current();
    auto* chip = new QWidget(parent);
    auto* h = new QHBoxLayout(chip);
    h->setContentsMargins(10, 3, 5, 3);
    h->setSpacing(4);
    chip->setStyleSheet(QStringLiteral(
        "QWidget{background:%1;border:1px solid %2;border-radius:11px;}")
        .arg(p.panel3.name(), p.border.name()));

    auto* label = new QLabel(text, chip);
    label->setStyleSheet(QStringLiteral("color:%1;border:none;").arg(p.text2.name()));
    h->addWidget(label);

    auto* del = new QToolButton(chip);
    del->setText(QStringLiteral("×"));
    del->setCursor(Qt::PointingHandCursor);
    del->setStyleSheet(QStringLiteral(
        "QToolButton{border:none;background:transparent;color:%1;"
        "font-weight:bold;padding:0 2px;}"
        "QToolButton:hover{color:%2;}")
        .arg(p.text3.name(), p.text.name()));
    QObject::connect(del, &QToolButton::clicked, chip, onRemove);
    h->addWidget(del);

    return chip;
}

} // namespace

EditMovieDialog::EditMovieDialog(const Movie& movie, QWidget* parent)
    : QDialog(parent), m_movie(movie), m_genres(movie.genres)
{
    setWindowTitle(tr("Edit Movie"));
    setModal(true);
    resize(560, 640);
    buildUi_();
}

void EditMovieDialog::buildUi_()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    // ---- Details group -----------------------------------------------------
    auto* box  = new QGroupBox(tr("Details"), this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    int r = 0;
    m_title = new QLineEdit(m_movie.title);
    grid->addWidget(fieldLabel(tr("Title")), r, 0);
    grid->addWidget(m_title, r, 1, 1, 3);
    ++r;

    m_original = new QLineEdit(m_movie.originalTitle);
    grid->addWidget(fieldLabel(tr("Original title")), r, 0);
    grid->addWidget(m_original, r, 1, 1, 3);
    ++r;

    m_sortTitle = new QLineEdit(m_movie.sortTitle);
    m_sortTitle->setPlaceholderText(tr("Defaults to the title when empty"));
    grid->addWidget(fieldLabel(tr("Sort title")), r, 0);
    grid->addWidget(m_sortTitle, r, 1, 1, 3);
    ++r;

    m_year = new QSpinBox;
    m_year->setRange(0, 2100);
    m_year->setSpecialValueText(tr("Unknown"));   // shown at 0
    m_year->setValue(m_movie.productionYear);
    m_runtime = new QSpinBox;
    m_runtime->setRange(0, 600);
    m_runtime->setSuffix(tr(" min"));
    m_runtime->setSpecialValueText(tr("Unknown"));
    m_runtime->setValue(m_movie.runningTimeMinutes);
    grid->addWidget(fieldLabel(tr("Year")), r, 0);
    grid->addWidget(m_year, r, 1);
    grid->addWidget(fieldLabel(tr("Runtime")), r, 2);
    grid->addWidget(m_runtime, r, 3);
    ++r;

    m_format = new QComboBox;
    m_format->addItem(tr("DVD"),      QVariant(QStringLiteral("DVD")));
    m_format->addItem(tr("Blu-ray"),  QVariant(QStringLiteral("BluRay")));
    m_format->addItem(tr("Ultra HD"), QVariant(QStringLiteral("UHD")));
    {
        const int idx = m_format->findData(m_movie.format);
        if (idx >= 0) {
            m_format->setCurrentIndex(idx);
        } else if (!m_movie.format.isEmpty()) {
            // Preserve an unrecognised stored format rather than silently
            // changing it to DVD.
            m_format->addItem(m_movie.format, QVariant(m_movie.format));
            m_format->setCurrentIndex(m_format->count() - 1);
        }
    }
    m_rating = new QSpinBox;
    m_rating->setRange(0, 10);
    m_rating->setSpecialValueText(tr("Unrated"));
    m_rating->setValue(m_movie.review.film);
    grid->addWidget(fieldLabel(tr("Format")), r, 0);
    grid->addWidget(m_format, r, 1);
    grid->addWidget(fieldLabel(tr("My rating")), r, 2);
    grid->addWidget(m_rating, r, 3);
    ++r;

    m_location = new QLineEdit(m_movie.locationId);
    m_purchaseDate = new QDateEdit;
    m_purchaseDate->setCalendarPopup(true);
    m_purchaseDate->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_purchaseDate->setMinimumDate(kNoDate);
    m_purchaseDate->setSpecialValueText(tr("Not set"));   // shown at kNoDate
    m_purchaseDate->setDate(m_movie.purchase.date.isValid()
                                ? m_movie.purchase.date : kNoDate);
    grid->addWidget(fieldLabel(tr("Location")), r, 0);
    grid->addWidget(m_location, r, 1);
    grid->addWidget(fieldLabel(tr("Purchased")), r, 2);
    grid->addWidget(m_purchaseDate, r, 3);
    ++r;

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    root->addWidget(box);

    // ---- Genres (chip editor) ---------------------------------------------
    root->addWidget(fieldLabel(tr("Genres")));
    m_genreHost = new QWidget;
    m_genreFlow = new FlowLayout(m_genreHost, 0, 6, 6);
    root->addWidget(m_genreHost);

    m_genreInput = new QLineEdit;
    m_genreInput->setPlaceholderText(tr("Add a genre and press Enter"));
    connect(m_genreInput, &QLineEdit::returnPressed, this, [this] {
        addGenre_(m_genreInput->text());
        m_genreInput->clear();
    });
    root->addWidget(m_genreInput);
    rebuildGenreChips_();

    // ---- Notes -------------------------------------------------------------
    root->addWidget(fieldLabel(tr("Notes")));
    m_notes = new QPlainTextEdit(m_movie.notes);
    m_notes->setMinimumHeight(90);
    root->addWidget(m_notes, 1);

    // ---- Buttons -----------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (auto* save = buttons->button(QDialogButtonBox::Save))
        save->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &EditMovieDialog::commit_);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void EditMovieDialog::addGenre_(const QString& genre)
{
    const QString g = genre.trimmed();
    if (g.isEmpty()) return;
    // Case-insensitive dedupe so "Action" isn't added twice.
    for (const QString& existing : m_genres)
        if (existing.compare(g, Qt::CaseInsensitive) == 0) return;
    m_genres << g;
    rebuildGenreChips_();
}

void EditMovieDialog::rebuildGenreChips_()
{
    // Clear existing chips.
    while (QLayoutItem* item = m_genreFlow->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }
    for (int i = 0; i < m_genres.size(); ++i) {
        const QString g = m_genres.at(i);
        m_genreFlow->addWidget(makeChip(g, m_genreHost, [this, g] {
            m_genres.removeAll(g);
            rebuildGenreChips_();
        }));
    }
    m_genreHost->setVisible(!m_genres.isEmpty());
}

void EditMovieDialog::commit_()
{
    m_movie.title         = m_title->text().trimmed();
    m_movie.originalTitle = m_original->text().trimmed();
    m_movie.sortTitle     = m_sortTitle->text().trimmed();
    m_movie.productionYear     = m_year->value();
    m_movie.runningTimeMinutes = m_runtime->value();
    m_movie.format        = m_format->currentData().toString();
    m_movie.review.film   = m_rating->value();
    m_movie.locationId    = m_location->text().trimmed();
    m_movie.genres        = m_genres;
    m_movie.notes         = m_notes->toPlainText();

    const QDate d = m_purchaseDate->date();
    m_movie.purchase.date = (d == kNoDate) ? QDate() : d;

    m_movie.lastEdited = QDateTime::currentDateTime();
    accept();
}

} // namespace xyz
