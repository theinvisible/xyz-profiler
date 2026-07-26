#include "ui/EditMovieDialog.h"

#include "tmdb/TmdbClient.h"
#include "ui/FlowLayout.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QPixmap>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QUrl>
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

QString joinValues(const QStringList& values)
{
    return values.join(QStringLiteral(", "));
}

QStringList splitValues(const QString& text)
{
    QStringList out;
    for (const QString& part : text.split(QChar(','), Qt::SkipEmptyParts)) {
        const QString value = part.trimmed();
        if (!value.isEmpty() && !out.contains(value, Qt::CaseInsensitive))
            out << value;
    }
    return out;
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
    : EditMovieDialog(movie, nullptr, false, parent)
{}

EditMovieDialog::EditMovieDialog(const Movie& movie, TmdbClient* tmdb, bool isNew,
                                 QWidget* parent)
    : QDialog(parent), m_movie(movie), m_tmdb(tmdb), m_isNew(isNew),
      m_genres(movie.genres)
{
    setWindowTitle(isNew ? tr("Add Movie") : tr("Edit Movie"));
    setModal(true);
    resize(760, 820);
    if (m_tmdb) {
        m_tmdbRequestId = static_cast<quint64>(reinterpret_cast<quintptr>(this));
        connect(m_tmdb, &TmdbClient::searchForFinished,
                this, &EditMovieDialog::onTmdbSearchFinished_);
        connect(m_tmdb, &TmdbClient::movieFinished,
                this, &EditMovieDialog::onTmdbMovieFinished_);
    }
    buildUi_();
}

void EditMovieDialog::buildUi_()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    auto* tabs = new QTabWidget(this);
    const auto makeTab = [this, tabs](const QString& title) {
        auto* scroll = new QScrollArea(tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);

        auto* body = new QWidget(scroll);
        auto* layout = new QVBoxLayout(body);
        layout->setContentsMargins(4, 8, 4, 4);
        layout->setSpacing(10);
        layout->addStretch();

        scroll->setWidget(body);
        tabs->addTab(scroll, title);
        return layout;
    };

    auto* detailsTab = makeTab(tr("Details"));
    auto* contentTab = makeTab(tr("Content"));
    auto* coversTab = makeTab(tr("Covers"));
    auto* tmdbTab = makeTab(tr("TMDb"));

    buildTmdbUi_(tmdbTab);

    // ---- Details -----------------------------------------------------------
    auto* box  = new QWidget(this);
    auto* grid = new QGridLayout(box);
    grid->setContentsMargins(0, 0, 0, 0);
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

    m_distTrait = new QLineEdit(m_movie.distTrait);
    grid->addWidget(fieldLabel(tr("Edition")), r, 0);
    grid->addWidget(m_distTrait, r, 1, 1, 3);
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

    m_ratingSystem = new QLineEdit(m_movie.rating.system);
    m_ratingValue = new QLineEdit(m_movie.rating.value);
    m_ratingAge = new QSpinBox;
    m_ratingAge->setRange(0, 99);
    m_ratingAge->setSpecialValueText(tr("Unknown"));
    m_ratingAge->setValue(m_movie.rating.age);
    grid->addWidget(fieldLabel(tr("Rating system")), r, 0);
    grid->addWidget(m_ratingSystem, r, 1);
    grid->addWidget(fieldLabel(tr("Rating")), r, 2);
    grid->addWidget(m_ratingValue, r, 3);
    ++r;

    // The remaining three review axes DP4 ships next to the film rating.
    const auto makeRatingSpin = [this](int value) {
        auto* s = new QSpinBox;
        s->setRange(0, 10);
        s->setSpecialValueText(tr("Unrated"));
        s->setValue(value);
        return s;
    };
    m_ratingVideo  = makeRatingSpin(m_movie.review.video);
    m_ratingAudio  = makeRatingSpin(m_movie.review.audio);
    m_ratingExtras = makeRatingSpin(m_movie.review.extras);
    grid->addWidget(fieldLabel(tr("Video rating")), r, 0);
    grid->addWidget(m_ratingVideo, r, 1);
    grid->addWidget(fieldLabel(tr("Audio rating")), r, 2);
    grid->addWidget(m_ratingAudio, r, 3);
    ++r;

    // Collection status. DP4 keeps wishlist entries in the same collection
    // file, so this is the only way to tell the two apart after an import.
    m_status = new QComboBox;
    m_status->addItem(tr("Owned"),    QVariant(QStringLiteral("Owned")));
    m_status->addItem(tr("Wishlist"), QVariant(QStringLiteral("Wishlist")));
    {
        const int idx = m_status->findData(m_movie.membership.type);
        if (idx >= 0) {
            m_status->setCurrentIndex(idx);
        } else if (!m_movie.membership.type.isEmpty()) {
            // DP4 has further types ("Order", "For Sale", …) — keep whatever
            // the source shipped instead of silently rewriting it to Owned.
            m_status->addItem(m_movie.membership.type,
                              QVariant(m_movie.membership.type));
            m_status->setCurrentIndex(m_status->count() - 1);
        }
    }
    grid->addWidget(fieldLabel(tr("Extras rating")), r, 0);
    grid->addWidget(m_ratingExtras, r, 1);
    grid->addWidget(fieldLabel(tr("Collection")), r, 2);
    grid->addWidget(m_status, r, 3);
    ++r;

    grid->addWidget(fieldLabel(tr("Minimum age")), r, 0);
    grid->addWidget(m_ratingAge, r, 1);
    ++r;

    m_studios = new QLineEdit(joinValues(m_movie.studios));
    m_mediaCompanies = new QLineEdit(joinValues(m_movie.mediaCompanies));
    grid->addWidget(fieldLabel(tr("Studios")), r, 0);
    grid->addWidget(m_studios, r, 1, 1, 3);
    ++r;
    grid->addWidget(fieldLabel(tr("Media companies")), r, 0);
    grid->addWidget(m_mediaCompanies, r, 1, 1, 3);
    ++r;

    m_countries = new QLineEdit(joinValues(m_movie.countriesOfOrigin));
    grid->addWidget(fieldLabel(tr("Countries")), r, 0);
    grid->addWidget(m_countries, r, 1, 1, 3);
    ++r;

    m_location = new QLineEdit(m_movie.locationId);
    m_purchaseDate = new QDateEdit;
    m_purchaseDate->setCalendarPopup(true);
    m_purchaseDate->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_purchaseDate->setMinimumDate(kNoDate);
    m_purchaseDate->setSpecialValueText(tr("Not set"));   // shown at kNoDate
    m_purchaseDate->setDate(m_movie.purchase.date.isValid()
                                ? m_movie.purchase.date : kNoDate);
    m_purchasePlace = new QLineEdit(m_movie.purchase.place);
    grid->addWidget(fieldLabel(tr("Location")), r, 0);
    grid->addWidget(m_location, r, 1);
    grid->addWidget(fieldLabel(tr("Purchased")), r, 2);
    grid->addWidget(m_purchaseDate, r, 3);
    ++r;
    grid->addWidget(fieldLabel(tr("Purchase place")), r, 0);
    grid->addWidget(m_purchasePlace, r, 1, 1, 3);
    ++r;

    // Amount + currency in one cell. The value stays a string so DP4's
    // locale-dependent bodies ("14.99", "12,99") round-trip untouched.
    const auto makeAmountEditor = [this](const MonetaryAmount& amount,
                                         QLineEdit** value, QLineEdit** currency) {
        auto* host = new QWidget;
        auto* lay = new QHBoxLayout(host);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(6);
        *value = new QLineEdit(amount.value);
        (*value)->setPlaceholderText(tr("Amount"));
        *currency = new QLineEdit(amount.denominationType);
        (*currency)->setPlaceholderText(tr("EUR"));
        (*currency)->setMaximumWidth(70);
        lay->addWidget(*value, 1);
        lay->addWidget(*currency);
        return host;
    };

    auto* priceEditor = makeAmountEditor(m_movie.purchase.price,
                                         &m_purchasePrice, &m_purchaseCurrency);
    auto* srpEditor = makeAmountEditor(m_movie.srp, &m_srp, &m_srpCurrency);
    grid->addWidget(fieldLabel(tr("Price paid")), r, 0);
    grid->addWidget(priceEditor, r, 1);
    grid->addWidget(fieldLabel(tr("SRP")), r, 2);
    grid->addWidget(srpEditor, r, 3);
    ++r;

    // DP4 bookkeeping fields. Rarely touched, but they are imported and were
    // previously unreachable, which reads as data loss to a migrating user.
    m_collectionNumber = new QSpinBox;
    m_collectionNumber->setRange(0, 999999);
    m_collectionNumber->setSpecialValueText(tr("None"));
    m_collectionNumber->setValue(m_movie.collectionNumber);
    m_countAs = new QSpinBox;
    m_countAs->setRange(0, 999);
    m_countAs->setToolTip(tr("How many titles this entry counts as in the "
                             "collection total"));
    m_countAs->setValue(m_movie.countAs);
    grid->addWidget(fieldLabel(tr("Collection no.")), r, 0);
    grid->addWidget(m_collectionNumber, r, 1);
    grid->addWidget(fieldLabel(tr("Counts as")), r, 2);
    grid->addWidget(m_countAs, r, 3);
    ++r;

    m_wishPriority = new QSpinBox;
    m_wishPriority->setRange(0, 99);
    m_wishPriority->setSpecialValueText(tr("None"));
    m_wishPriority->setValue(m_movie.wishPriority);
    grid->addWidget(fieldLabel(tr("Wish priority")), r, 0);
    grid->addWidget(m_wishPriority, r, 1);
    ++r;

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    detailsTab->insertWidget(detailsTab->count() - 1, box);

    // ---- Cover images ------------------------------------------------------
    auto* coverBox = new QWidget(this);
    auto* coverLayout = new QHBoxLayout(coverBox);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setSpacing(16);

    m_coverFrontPreview = new QLabel;
    m_coverBackPreview = new QLabel;
    coverLayout->addWidget(makeCoverPanel_(tr("Front cover"), m_coverFrontPreview,
                                           &m_chooseFrontCover, &m_clearFrontCover));
    coverLayout->addWidget(makeCoverPanel_(tr("Back cover"), m_coverBackPreview,
                                           &m_chooseBackCover, &m_clearBackCover));
    coverLayout->addStretch();

    connect(m_chooseFrontCover, &QPushButton::clicked, this,
            [this] { pickCover_(true); });
    connect(m_chooseBackCover, &QPushButton::clicked, this,
            [this] { pickCover_(false); });
    connect(m_clearFrontCover, &QPushButton::clicked, this, [this] {
        m_pendingFrontCoverPath.clear();
        m_tmdbPosterPath.clear();
        m_movie.coverFrontPath.clear();
        updateCoverPreview_(m_coverFrontPreview, {});
    });
    connect(m_clearBackCover, &QPushButton::clicked, this, [this] {
        m_pendingBackCoverPath.clear();
        m_movie.coverBackPath.clear();
        updateCoverPreview_(m_coverBackPreview, {});
    });

    coversTab->insertWidget(coversTab->count() - 1, coverBox);
    updateCoverPreview_(m_coverFrontPreview, m_movie.coverFrontPath);
    updateCoverPreview_(m_coverBackPreview, m_movie.coverBackPath);

    // ---- Genres (chip editor) ---------------------------------------------
    contentTab->insertWidget(contentTab->count() - 1, fieldLabel(tr("Genres")));
    m_genreHost = new QWidget;
    m_genreFlow = new FlowLayout(m_genreHost, 0, 6, 6);
    contentTab->insertWidget(contentTab->count() - 1, m_genreHost);

    m_genreInput = new QLineEdit;
    m_genreInput->setPlaceholderText(tr("Add a genre and press Enter"));
    connect(m_genreInput, &QLineEdit::returnPressed, this, [this] {
        addGenre_(m_genreInput->text());
        m_genreInput->clear();
    });
    contentTab->insertWidget(contentTab->count() - 1, m_genreInput);
    rebuildGenreChips_();

    // ---- Overview ----------------------------------------------------------
    contentTab->insertWidget(contentTab->count() - 1, fieldLabel(tr("Overview")));
    m_overview = new QPlainTextEdit(m_movie.overview);
    m_overview->setMinimumHeight(80);
    contentTab->insertWidget(contentTab->count() - 1, m_overview, 1);

    // ---- Notes -------------------------------------------------------------
    contentTab->insertWidget(contentTab->count() - 1, fieldLabel(tr("Notes")));
    m_notes = new QPlainTextEdit(m_movie.notes);
    m_notes->setMinimumHeight(70);
    contentTab->insertWidget(contentTab->count() - 1, m_notes, 1);

    // ---- Custom fields -----------------------------------------------------
    // DP4 power users keep a lot in here (watched dates, shelf slots, lending
    // notes). Order is positional in the DB, so the table's row order is the
    // stored order.
    contentTab->insertWidget(contentTab->count() - 1, fieldLabel(tr("Custom fields")));
    m_customFields = new QTableWidget(0, 2);
    m_customFields->setHorizontalHeaderLabels({tr("Name"), tr("Value")});
    m_customFields->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_customFields->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_customFields->verticalHeader()->setVisible(false);
    m_customFields->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_customFields->setMinimumHeight(110);
    for (const auto& field : m_movie.customFields) {
        const int row = m_customFields->rowCount();
        m_customFields->insertRow(row);
        m_customFields->setItem(row, 0, new QTableWidgetItem(field.name));
        m_customFields->setItem(row, 1, new QTableWidgetItem(field.value));
    }
    contentTab->insertWidget(contentTab->count() - 1, m_customFields, 1);

    {
        auto* buttons = new QWidget;
        auto* bl = new QHBoxLayout(buttons);
        bl->setContentsMargins(0, 0, 0, 0);
        bl->setSpacing(6);
        auto* addField = new QPushButton(tr("Add field"));
        auto* removeField = new QPushButton(tr("Remove field"));
        connect(addField, &QPushButton::clicked, this, [this] {
            const int row = m_customFields->rowCount();
            m_customFields->insertRow(row);
            m_customFields->setItem(row, 0, new QTableWidgetItem);
            m_customFields->setItem(row, 1, new QTableWidgetItem);
            m_customFields->editItem(m_customFields->item(row, 0));
        });
        connect(removeField, &QPushButton::clicked, this, [this] {
            const int row = m_customFields->currentRow();
            if (row >= 0) m_customFields->removeRow(row);
        });
        bl->addWidget(addField);
        bl->addWidget(removeField);
        bl->addStretch();
        contentTab->insertWidget(contentTab->count() - 1, buttons);
    }

    // ---- My links ----------------------------------------------------------
    // DP4's <MyLinks> is a free-text body of unclear shape, so it is edited
    // and stored verbatim rather than parsed into structured link entries.
    contentTab->insertWidget(contentTab->count() - 1, fieldLabel(tr("My links")));
    m_myLinks = new QPlainTextEdit(m_movie.myLinks);
    m_myLinks->setMinimumHeight(55);
    contentTab->insertWidget(contentTab->count() - 1, m_myLinks, 1);

    root->addWidget(tabs, 1);

    // ---- Buttons -----------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (auto* save = buttons->button(QDialogButtonBox::Save))
        save->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &EditMovieDialog::commit_);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void EditMovieDialog::buildTmdbUi_(QVBoxLayout* root)
{
    const auto insertIndex = [root] {
        return root->count() > 0 ? root->count() - 1 : 0;
    };

    if (!m_tmdb || !m_tmdb->hasApiKey()) {
        auto* label = new QLabel(
            tr("TMDb is not configured. You can still enter and save the title manually."));
        label->setWordWrap(true);
        label->setEnabled(false);
        root->insertWidget(insertIndex(), label);
        return;
    }

    auto* searchRow = new QHBoxLayout;
    searchRow->setSpacing(8);
    m_tmdbSearch = new QLineEdit(m_movie.title);
    m_tmdbSearch->setPlaceholderText(tr("Search TMDb by title"));
    auto* searchBtn = new QPushButton(tr("Search"));
    connect(searchBtn, &QPushButton::clicked, this, &EditMovieDialog::runTmdbSearch_);
    connect(m_tmdbSearch, &QLineEdit::returnPressed, this, &EditMovieDialog::runTmdbSearch_);
    searchRow->addWidget(m_tmdbSearch, 1);
    searchRow->addWidget(searchBtn);
    root->insertLayout(insertIndex(), searchRow);

    m_tmdbStatus = new QLabel(tr("Search TMDb to prefill fields and link this title."));
    m_tmdbStatus->setWordWrap(true);
    m_tmdbStatus->setEnabled(false);
    root->insertWidget(insertIndex(), m_tmdbStatus);

    m_tmdbResults = new QListWidget;
    m_tmdbResults->setMinimumHeight(260);
    m_tmdbResults->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tmdbResults->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_tmdbResults, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* cur, QListWidgetItem*) {
        if (m_tmdbApply)
            m_tmdbApply->setEnabled(cur && cur->data(Qt::UserRole).toInt() > 0);
    });
    root->insertWidget(insertIndex(), m_tmdbResults, 1);

    auto* actionRow = new QHBoxLayout;
    actionRow->setSpacing(8);
    actionRow->addStretch();
    m_tmdbApply = new QPushButton(tr("Apply selected TMDb data"));
    m_tmdbApply->setEnabled(false);
    connect(m_tmdbApply, &QPushButton::clicked, this, [this] {
        if (!m_tmdb || !m_tmdbResults || !m_tmdbResults->currentItem()) return;
        const int tmdbId = m_tmdbResults->currentItem()->data(Qt::UserRole).toInt();
        if (tmdbId <= 0) return;
        m_pendingTmdbId = tmdbId;
        m_tmdbPosterPath.clear();
        for (const auto& c : m_tmdbCandidates) {
            if (c.id == tmdbId) {
                m_tmdbPosterPath = c.posterPath;
                break;
            }
        }
        if (m_tmdbStatus) m_tmdbStatus->setText(tr("Loading TMDb details..."));
        m_tmdb->getMovie(tmdbId);
    });
    actionRow->addWidget(m_tmdbApply);
    root->insertLayout(insertIndex(), actionRow);
}

void EditMovieDialog::runTmdbSearch_()
{
    if (!m_tmdb || !m_tmdbSearch) return;
    const QString title = m_tmdbSearch->text().trimmed();
    if (title.isEmpty()) return;

    m_tmdbCandidates.clear();
    if (m_tmdbResults) m_tmdbResults->clear();
    if (m_tmdbApply) m_tmdbApply->setEnabled(false);
    if (m_tmdbStatus) m_tmdbStatus->setText(tr("Searching TMDb..."));
    m_tmdb->searchFor(m_tmdbRequestId, title, m_year ? m_year->value() : 0);
}

void EditMovieDialog::onTmdbSearchFinished_(quint64 requestId,
                                            const QList<TmdbCandidate>& candidates,
                                            const QString& error)
{
    if (requestId != m_tmdbRequestId || !m_tmdbResults) return;
    m_tmdbCandidates = candidates;
    m_tmdbResults->clear();
    if (m_tmdbApply) m_tmdbApply->setEnabled(false);

    if (!error.isEmpty()) {
        if (m_tmdbStatus) m_tmdbStatus->setText(error);
        return;
    }
    if (candidates.isEmpty()) {
        if (m_tmdbStatus) m_tmdbStatus->setText(tr("No TMDb matches found."));
        return;
    }

    if (m_tmdbStatus) m_tmdbStatus->setText(tr("Select a match, then apply it."));
    for (const auto& c : candidates) {
        auto* row = makeTmdbResultRow_(c);
        auto* item = new QListWidgetItem(m_tmdbResults);
        item->setData(Qt::UserRole, c.id);
        item->setSizeHint(row->sizeHint());
        m_tmdbResults->setItemWidget(item, row);
    }
    if (m_tmdbResults->count() > 0)
        m_tmdbResults->setCurrentRow(0);
}

void EditMovieDialog::onTmdbMovieFinished_(const TmdbMovieDetails& details,
                                           const QString& error)
{
    if (m_pendingTmdbId <= 0) return;
    const int expected = m_pendingTmdbId;
    m_pendingTmdbId = 0;

    if (!error.isEmpty()) {
        if (m_tmdbStatus) m_tmdbStatus->setText(error);
        return;
    }
    if (details.id != expected) return;

    applyTmdbDetails_(details);
    if (m_tmdbStatus)
        m_tmdbStatus->setText(tr("TMDb data applied. Review the fields before saving."));
}

void EditMovieDialog::applyTmdbDetails_(const TmdbMovieDetails& d)
{
    if (m_title) m_title->setText(d.title);
    if (m_original) m_original->setText(d.originalTitle);
    if (m_sortTitle && m_sortTitle->text().trimmed().isEmpty())
        m_sortTitle->setText(d.title);
    if (m_year && d.releaseDate.size() >= 4)
        m_year->setValue(d.releaseDate.left(4).toInt());
    if (m_runtime) m_runtime->setValue(d.runtime);
    if (m_studios) m_studios->setText(joinValues(d.productionCompanies));
    if (m_countries) m_countries->setText(joinValues(d.productionCountries));
    if (m_overview) m_overview->setPlainText(d.overview);

    m_genres = d.genres;
    rebuildGenreChips_();
    m_movie.tmdbId = d.id;
    if (!d.posterPath.isEmpty()) {
        m_tmdbPosterPath = d.posterPath;
        loadTmdbPosterPreview_(d.posterPath);
    }
}

QWidget* EditMovieDialog::makeTmdbResultRow_(const TmdbCandidate& c)
{
    auto* widget = new QWidget(m_tmdbResults);
    auto* row = new QHBoxLayout(widget);
    row->setContentsMargins(6, 6, 6, 6);
    row->setSpacing(10);

    auto* poster = new QLabel(widget);
    poster->setFixedSize(74, 110);
    poster->setAlignment(Qt::AlignCenter);
    QPixmap placeholder(poster->size());
    placeholder.fill(QColor(70, 70, 70));
    poster->setPixmap(placeholder);
    row->addWidget(poster, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(3);

    QString titleText = c.title;
    if (c.year() > 0)
        titleText += QStringLiteral(" (%1)").arg(c.year());
    auto* title = new QLabel(titleText, widget);
    {
        QFont f = title->font();
        f.setPointSize(12);
        f.setBold(true);
        title->setFont(f);
    }
    title->setWordWrap(true);
    text->addWidget(title);

    if (!c.originalTitle.isEmpty() && c.originalTitle != c.title) {
        auto* original = new QLabel(c.originalTitle, widget);
        QFont f = original->font();
        f.setItalic(true);
        original->setFont(f);
        original->setWordWrap(true);
        text->addWidget(original);
    }

    if (!c.overview.isEmpty()) {
        auto* overview = new QLabel(c.overview, widget);
        overview->setWordWrap(true);
        overview->setMaximumHeight(48);
        text->addWidget(overview);
    }

    QStringList meta;
    if (!c.originalLanguage.isEmpty())
        meta << c.originalLanguage.toUpper();
    if (c.voteAverage > 0.0)
        meta << QStringLiteral("★ %1").arg(c.voteAverage, 0, 'f', 1);
    if (c.voteCount > 0)
        meta << tr("%n vote(s)", "", c.voteCount);
    meta << QStringLiteral("TMDb #%1").arg(c.id);

    auto* details = new QLabel(meta.join(QStringLiteral(" · ")), widget);
    details->setEnabled(false);
    text->addWidget(details);
    text->addStretch();
    row->addLayout(text, 1);

    if (!c.posterPath.isEmpty() && m_tmdb && m_tmdb->network()) {
        const QString url = m_tmdb->imageUrl(c.posterPath, QStringLiteral("w185"));
        if (!url.isEmpty()) {
            QPointer<QLabel> guard(poster);
            auto* reply = m_tmdb->network()->get(QNetworkRequest(QUrl(url)));
            connect(reply, &QNetworkReply::finished, this, [reply, guard]() {
                reply->deleteLater();
                if (!guard || reply->error() != QNetworkReply::NoError) return;
                QPixmap pix;
                pix.loadFromData(reply->readAll());
                if (!pix.isNull())
                    guard->setPixmap(pix.scaled(74, 110, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation));
            });
        }
    }

    return widget;
}

QWidget* EditMovieDialog::makeCoverPanel_(const QString& title, QLabel* preview,
                                          QPushButton** chooseButton,
                                          QPushButton** clearButton)
{
    auto* panel = new QWidget(this);
    panel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title, panel);
    {
        QFont f = titleLabel->font();
        f.setBold(true);
        titleLabel->setFont(f);
    }
    layout->addWidget(titleLabel);

    preview->setFixedSize(180, 270);
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet(QStringLiteral("border:1px solid %1;background:%2;")
        .arg(Theme::current().border.name(), Theme::current().panel2.name()));
    layout->addWidget(preview);

    auto* choose = new QPushButton(tr("Choose image..."), panel);
    auto* clear = new QPushButton(tr("Clear"), panel);
    layout->addWidget(choose);
    layout->addWidget(clear);
    layout->addStretch();

    if (chooseButton) *chooseButton = choose;
    if (clearButton) *clearButton = clear;
    return panel;
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

void EditMovieDialog::updateCoverPreview_(QLabel* label, const QString& path)
{
    if (!label) return;
    QPixmap placeholder(label->size());
    placeholder.fill(QColor(70, 70, 70));

    QPixmap pix(path);
    if (pix.isNull()) {
        label->setPixmap(placeholder);
        return;
    }
    label->setPixmap(pix.scaled(label->size(), Qt::KeepAspectRatio,
                                Qt::SmoothTransformation));
}

void EditMovieDialog::loadTmdbPosterPreview_(const QString& posterPath)
{
    if (!m_tmdb || !m_tmdb->network() || posterPath.isEmpty() || !m_coverFrontPreview)
        return;

    const QString url = m_tmdb->imageUrl(posterPath, QStringLiteral("w342"));
    if (url.isEmpty()) return;

    QPointer<QLabel> guard(m_coverFrontPreview);
    auto* reply = m_tmdb->network()->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [reply, guard]() {
        reply->deleteLater();
        if (!guard || reply->error() != QNetworkReply::NoError) return;
        QPixmap pix;
        pix.loadFromData(reply->readAll());
        if (!pix.isNull())
            guard->setPixmap(pix.scaled(guard->size(), Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
    });
}

void EditMovieDialog::pickCover_(bool front)
{
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select cover image"), {},
        tr("Images (*.jpg *.jpeg *.png *.webp *.bmp);;All files (*.*)"));
    if (file.isEmpty()) return;

    if (front) {
        m_pendingFrontCoverPath = file;
        m_tmdbPosterPath.clear();
        updateCoverPreview_(m_coverFrontPreview, file);
    } else {
        m_pendingBackCoverPath = file;
        updateCoverPreview_(m_coverBackPreview, file);
    }
}

void EditMovieDialog::commit_()
{
    m_movie.title         = m_title->text().trimmed();
    if (m_movie.title.isEmpty()) {
        m_title->setFocus();
        return;
    }
    m_movie.originalTitle = m_original->text().trimmed();
    m_movie.sortTitle     = m_sortTitle->text().trimmed();
    m_movie.distTrait     = m_distTrait->text().trimmed();
    m_movie.productionYear     = m_year->value();
    m_movie.runningTimeMinutes = m_runtime->value();
    m_movie.format        = m_format->currentData().toString();
    m_movie.rating.system = m_ratingSystem->text().trimmed();
    m_movie.rating.value  = m_ratingValue->text().trimmed();
    m_movie.rating.age    = m_ratingAge->value();
    m_movie.review.film   = m_rating->value();
    m_movie.studios       = splitValues(m_studios->text());
    m_movie.mediaCompanies = splitValues(m_mediaCompanies->text());
    m_movie.countriesOfOrigin = splitValues(m_countries->text());
    m_movie.locationId    = m_location->text().trimmed();
    m_movie.purchase.place = m_purchasePlace->text().trimmed();
    m_movie.genres        = m_genres;
    m_movie.overview      = m_overview->toPlainText();
    m_movie.notes         = m_notes->toPlainText();

    m_movie.review.video  = m_ratingVideo->value();
    m_movie.review.audio  = m_ratingAudio->value();
    m_movie.review.extras = m_ratingExtras->value();

    // Rebuild from the table. A row with no name has nothing to key the value
    // on, so it is dropped rather than stored as an unnamed field.
    m_movie.customFields.clear();
    for (int row = 0; row < m_customFields->rowCount(); ++row) {
        const QTableWidgetItem* nameItem  = m_customFields->item(row, 0);
        const QTableWidgetItem* valueItem = m_customFields->item(row, 1);
        const QString name = nameItem ? nameItem->text().trimmed() : QString();
        if (name.isEmpty()) continue;
        CustomField field;
        field.name  = name;
        field.value = valueItem ? valueItem->text() : QString();
        m_movie.customFields << field;
    }

    m_movie.collectionNumber = m_collectionNumber->value();
    m_movie.countAs          = m_countAs->value();
    m_movie.wishPriority     = m_wishPriority->value();
    m_movie.myLinks          = m_myLinks->toPlainText();

    const QDate d = m_purchaseDate->date();
    m_movie.purchase.date = (d == kNoDate) ? QDate() : d;

    // formattedValue is the *source's* locale formatting and goes stale the
    // moment the amount or currency changes. Clear it on edit and let
    // displayAmount() fall back to "value currency" — synthesising a
    // replacement would guess at conventions we don't know.
    const auto applyAmount = [](MonetaryAmount& amount, const QLineEdit* value,
                                const QLineEdit* currency) {
        const QString nextValue    = value->text().trimmed();
        const QString nextCurrency = currency->text().trimmed();
        if (nextValue == amount.value && nextCurrency == amount.denominationType)
            return;
        amount.value            = nextValue;
        amount.denominationType = nextCurrency;
        amount.formattedValue.clear();
        amount.denominationDescription.clear();
    };
    applyAmount(m_movie.purchase.price, m_purchasePrice, m_purchaseCurrency);
    applyAmount(m_movie.srp, m_srp, m_srpCurrency);

    // Only re-derive IsPartOfOwnedCollection when the user actually changed
    // the status — otherwise an imported "Order" entry would silently be
    // reclassified as owned on the next unrelated edit.
    const QString status = m_status->currentData().toString();
    if (status != m_movie.membership.type) {
        m_movie.membership.type = status;
        m_movie.membership.isPartOfOwnedCollection =
            !isWishlistMembership(m_movie.membership);
    }
    if (m_movie.membership.type.isEmpty())
        m_movie.membership.type = QStringLiteral("Owned");
    if (!m_movie.profileTimestamp.isValid())
        m_movie.profileTimestamp = QDateTime::currentDateTime();
    m_movie.lastEdited = QDateTime::currentDateTime();
    accept();
}

} // namespace xyz
