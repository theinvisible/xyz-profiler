#include "ui/AddTitleDialog.h"

#include "tmdb/TmdbClient.h"
#include "tmdb/TmdbTypes.h"

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

namespace xyz {
namespace {

// Curated short lists — full ISO tables would be unwieldy in a combo box.
struct CodeName { const char* code; const char* name; };

const CodeName kLanguages[] = {
    {"",   "Any language"}, {"de", "German"},  {"en", "English"}, {"fr", "French"},
    {"it", "Italian"},      {"es", "Spanish"}, {"ja", "Japanese"},{"ko", "Korean"},
    {"zh", "Chinese"},      {"ru", "Russian"}, {"sv", "Swedish"}, {"da", "Danish"},
};

const CodeName kCountries[] = {
    {"",   "Any country"}, {"US", "USA"},      {"GB", "UK"},      {"DE", "Germany"},
    {"FR", "France"},      {"IT", "Italy"},    {"JP", "Japan"},   {"KR", "South Korea"},
    {"CA", "Canada"},      {"AU", "Australia"},{"ES", "Spain"},   {"AT", "Austria"},
};

struct SortOption { const char* sortBy; const char* label; };
const SortOption kSortOptions[] = {
    {"popularity.desc",            "Popularity"},
    {"vote_average.desc",          "Rating"},
    {"primary_release_date.desc",  "Release date (newest)"},
    {"primary_release_date.asc",   "Release date (oldest)"},
    {"original_title.asc",         "Title (A-Z)"},
};

} // namespace

AddTitleDialog::AddTitleDialog(TmdbClient* tmdb, QWidget* parent)
    : QDialog(parent), m_tmdb(tmdb)
{
    setWindowTitle(tr("Add a Title"));
    setModal(true);
    resize(780, 700);
    buildUi_();
    buildGenreMenu_();

    if (m_tmdb)
        connect(m_tmdb, &TmdbClient::discoverFinished,
                this, &AddTitleDialog::onResults_);
}

void AddTitleDialog::buildUi_()
{
    auto* root = new QVBoxLayout(this);

    // ---- Criteria ----------------------------------------------------------
    auto* box  = new QGroupBox(tr("Search criteria"), this);
    auto* grid = new QGridLayout(box);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    int r = 0;
    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText(tr("Title (leave empty to browse by filters only)"));
    grid->addWidget(new QLabel(tr("Title")), r, 0);
    grid->addWidget(m_titleEdit, r, 1, 1, 3);
    ++r;

    const auto yearSpin = [] {
        auto* s = new QSpinBox;
        s->setRange(0, 2100);
        s->setSpecialValueText(tr("Any"));   // shown when value == 0
        s->setValue(0);
        return s;
    };
    m_yearFrom = yearSpin();
    m_yearTo   = yearSpin();
    grid->addWidget(new QLabel(tr("Year from")), r, 0);
    grid->addWidget(m_yearFrom, r, 1);
    grid->addWidget(new QLabel(tr("Year to")),   r, 2);
    grid->addWidget(m_yearTo,   r, 3);
    ++r;

    m_genreBtn = new QToolButton;
    m_genreBtn->setText(tr("Any genre"));
    m_genreBtn->setPopupMode(QToolButton::InstantPopup);
    m_genreBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_genreBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_personEdit = new QLineEdit;
    m_personEdit->setPlaceholderText(tr("Actor or director"));
    grid->addWidget(new QLabel(tr("Genres")), r, 0);
    grid->addWidget(m_genreBtn, r, 1);
    grid->addWidget(new QLabel(tr("Person")), r, 2);
    grid->addWidget(m_personEdit, r, 3);
    ++r;

    m_minRating = new QDoubleSpinBox;
    m_minRating->setRange(0.0, 10.0);
    m_minRating->setSingleStep(0.5);
    m_minRating->setDecimals(1);
    m_minRating->setSpecialValueText(tr("Any"));
    m_langCombo = new QComboBox;
    for (const auto& l : kLanguages)
        m_langCombo->addItem(tr(l.name), QVariant(QString::fromLatin1(l.code)));
    grid->addWidget(new QLabel(tr("Min. rating")), r, 0);
    grid->addWidget(m_minRating, r, 1);
    grid->addWidget(new QLabel(tr("Orig. language")), r, 2);
    grid->addWidget(m_langCombo, r, 3);
    ++r;

    m_countryCombo = new QComboBox;
    for (const auto& c : kCountries)
        m_countryCombo->addItem(tr(c.name), QVariant(QString::fromLatin1(c.code)));
    m_sortCombo = new QComboBox;
    for (const auto& s : kSortOptions)
        m_sortCombo->addItem(tr(s.label), QVariant(QString::fromLatin1(s.sortBy)));
    grid->addWidget(new QLabel(tr("Country")), r, 0);
    grid->addWidget(m_countryCombo, r, 1);
    grid->addWidget(new QLabel(tr("Sort by")), r, 2);
    grid->addWidget(m_sortCombo, r, 3);
    ++r;

    const auto minuteSpin = [] {
        auto* s = new QSpinBox;
        s->setRange(0, 600);
        s->setSuffix(tr(" min"));
        s->setSpecialValueText(tr("Any"));
        s->setValue(0);
        return s;
    };
    m_runtimeMin = minuteSpin();
    m_runtimeMax = minuteSpin();
    grid->addWidget(new QLabel(tr("Runtime from")), r, 0);
    grid->addWidget(m_runtimeMin, r, 1);
    grid->addWidget(new QLabel(tr("Runtime to")),   r, 2);
    grid->addWidget(m_runtimeMax, r, 3);
    ++r;

    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    root->addWidget(box);

    auto* hint = new QLabel(
        tr("With a title, filters refine the text results; without a title, "
           "all filters are applied. Runtime, country and person work in "
           "filter-only mode."));
    hint->setWordWrap(true);
    { QFont f = hint->font(); f.setPointSizeF(f.pointSizeF() - 1.0); hint->setFont(f); }
    hint->setEnabled(false);
    root->addWidget(hint);

    auto* searchRow = new QHBoxLayout;
    searchRow->addStretch();
    m_searchBtn = new QPushButton(tr("Search"));
    m_searchBtn->setDefault(true);
    connect(m_searchBtn, &QPushButton::clicked, this, &AddTitleDialog::runSearch_);
    connect(m_titleEdit, &QLineEdit::returnPressed, this, &AddTitleDialog::runSearch_);
    searchRow->addWidget(m_searchBtn);
    root->addLayout(searchRow);

    // ---- Results -----------------------------------------------------------
    m_statusLabel = new QLabel(tr("Enter criteria and press Search."));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(m_statusLabel);

    m_listWidget = new QListWidget;
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_listWidget, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
        m_selectedTmdbId = item->data(Qt::UserRole).toInt();
        m_selectedPosterPath.clear();
        for (const auto& c : m_candidates)
            if (c.id == m_selectedTmdbId) { m_selectedPosterPath = c.posterPath; break; }
        m_addBtn->setEnabled(m_selectedTmdbId > 0);
    });
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
        m_selectedTmdbId = item->data(Qt::UserRole).toInt();
        for (const auto& c : m_candidates)
            if (c.id == m_selectedTmdbId) { m_selectedPosterPath = c.posterPath; break; }
        if (m_selectedTmdbId > 0) accept();
    });
    root->addWidget(m_listWidget, 1);

    // ---- Buttons -----------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_addBtn = buttons->addButton(tr("Add to collection"), QDialogButtonBox::AcceptRole);
    m_addBtn->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void AddTitleDialog::buildGenreMenu_()
{
    m_genreMenu = new QMenu(this);
    for (const auto& g : TmdbClient::movieGenres()) {
        auto* act = m_genreMenu->addAction(g.name);
        act->setCheckable(true);
        const int id = g.id;
        connect(act, &QAction::toggled, this, [this, id](bool on) {
            if (on) {
                if (!m_selectedGenreIds.contains(id)) m_selectedGenreIds << id;
            } else {
                m_selectedGenreIds.removeAll(id);
            }
            m_genreBtn->setText(m_selectedGenreIds.isEmpty()
                ? tr("Any genre")
                : tr("%n genre(s)", "", int(m_selectedGenreIds.size())));
        });
    }
    m_genreBtn->setMenu(m_genreMenu);
}

void AddTitleDialog::runSearch_()
{
    if (!m_tmdb) return;

    TmdbDiscoverQuery q;
    q.title            = m_titleEdit->text();
    q.yearMin          = m_yearFrom->value();
    q.yearMax          = m_yearTo->value();
    q.genreIds         = m_selectedGenreIds;
    q.person           = m_personEdit->text();
    q.voteAverageMin   = m_minRating->value();
    q.originalLanguage = m_langCombo->currentData().toString();
    q.originCountry    = m_countryCombo->currentData().toString();
    q.runtimeMin       = m_runtimeMin->value();
    q.runtimeMax       = m_runtimeMax->value();
    q.sortBy           = m_sortCombo->currentData().toString();

    m_candidates.clear();
    m_listWidget->clear();
    m_posterLabels.clear();
    m_posterUrls.clear();
    m_selectedTmdbId = 0;
    m_selectedPosterPath.clear();
    m_addBtn->setEnabled(false);

    m_statusLabel->setText(tr("Searching TMDb..."));
    m_statusLabel->setVisible(true);
    m_tmdb->discover(q);
}

void AddTitleDialog::onResults_(const QList<TmdbCandidate>& candidates,
                                const QString& error)
{
    m_candidates = candidates;
    m_listWidget->clear();
    m_posterLabels.clear();
    m_posterUrls.clear();
    m_selectedTmdbId = 0;
    m_addBtn->setEnabled(false);

    if (!error.isEmpty()) {
        m_statusLabel->setText(error);
        m_statusLabel->setVisible(true);
        return;
    }
    if (candidates.isEmpty()) {
        m_statusLabel->setText(tr("No matches found."));
        m_statusLabel->setVisible(true);
        return;
    }
    m_statusLabel->setVisible(false);

    for (const auto& c : candidates) {
        auto* row  = makeResultRow_(c);
        auto* item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, c.id);
        item->setSizeHint(row->sizeHint());
        m_listWidget->setItemWidget(item, row);

        if (!c.posterPath.isEmpty() && m_tmdb)
            m_posterUrls.append(m_tmdb->imageUrl(c.posterPath, QStringLiteral("w185")));
        else
            m_posterUrls.append(QString());
    }

    QNetworkAccessManager* nam = m_tmdb ? m_tmdb->network() : nullptr;
    if (!nam) return;
    for (int i = 0; i < m_posterUrls.size(); ++i) {
        const QString& url = m_posterUrls.at(i);
        if (url.isEmpty() || i >= m_posterLabels.size()) continue;
        // QPointer guards against a newer search (or dialog close) deleting
        // this row's QLabel while the download is still in flight — writing to
        // the freed widget was the access-violation crash.
        QPointer<QLabel> label = m_posterLabels.at(i);
        auto* reply = nam->get(QNetworkRequest(QUrl(url)));
        connect(reply, &QNetworkReply::finished, this, [reply, label]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            if (!label) return;
            QPixmap poster;
            poster.loadFromData(reply->readAll());
            if (!poster.isNull())
                label->setPixmap(poster.scaled(74, 110, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
        });
    }
}

QWidget* AddTitleDialog::makeResultRow_(const TmdbCandidate& c)
{
    auto* widget = new QWidget(m_listWidget);
    auto* h = new QHBoxLayout(widget);
    h->setContentsMargins(4, 4, 4, 4);

    auto* poster = new QLabel(widget);
    poster->setFixedSize(74, 110);
    poster->setAlignment(Qt::AlignCenter);
    QPixmap placeholder(74, 110);
    placeholder.fill(QColor(80, 80, 80));
    poster->setPixmap(placeholder);
    h->addWidget(poster, 0, Qt::AlignTop);
    m_posterLabels.append(poster);

    auto* text = new QVBoxLayout;
    text->setSpacing(2);

    auto* title = new QLabel(widget);
    {
        QString t = c.title;
        if (c.year() > 0) t += QStringLiteral(" (%1)").arg(c.year());
        title->setText(t);
    }
    { QFont f = title->font(); f.setPointSize(13); f.setBold(true); title->setFont(f); }
    title->setWordWrap(true);
    text->addWidget(title);

    if (!c.originalTitle.isEmpty() && c.originalTitle != c.title) {
        auto* orig = new QLabel(c.originalTitle, widget);
        QFont f = orig->font(); f.setItalic(true); orig->setFont(f);
        orig->setWordWrap(true);
        text->addWidget(orig);
    }

    if (!c.overview.isEmpty()) {
        auto* ov = new QLabel(c.overview, widget);
        QFont f = ov->font(); f.setPointSize(11); ov->setFont(f);
        ov->setWordWrap(true);
        ov->setMaximumHeight(56);
        text->addWidget(ov);
    }

    {
        auto* votes = new QLabel(
            QStringLiteral("★ %1 · %2 votes · TMDb #%3")
                .arg(c.voteAverage, 0, 'f', 1).arg(c.voteCount).arg(c.id),
            widget);
        QFont f = votes->font(); f.setPointSize(9); votes->setFont(f);
        text->addWidget(votes);
    }

    text->addStretch();
    h->addLayout(text, 1);
    return widget;
}

} // namespace xyz
