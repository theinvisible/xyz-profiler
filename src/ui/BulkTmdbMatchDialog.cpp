#include "ui/BulkTmdbMatchDialog.h"

#include "tmdb/TmdbClient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

namespace xyz {
namespace {

constexpr int kMaxInFlight = 6;     // matches QNAM's default per-host pool
constexpr int kPosterW = 48;
constexpr int kPosterH = 72;

// Combo item data roles for a candidate's TMDb id and poster path.
constexpr int kRoleTmdbId  = Qt::UserRole + 1;
constexpr int kRolePoster  = Qt::UserRole + 2;

enum Column { ColPick = 0, ColTitle, ColMatch, ColPoster, ColCount };

} // namespace

BulkTmdbMatchDialog::BulkTmdbMatchDialog(TmdbClient* tmdb,
                                         const QList<Movie>& movies,
                                         QWidget* parent)
    : QDialog(parent), m_tmdb(tmdb), m_movies(movies)
{
    setWindowTitle(tr("Match on TMDb"));
    setModal(true);
    resize(820, 640);
    buildUi_();
    startSearches_();
}

void BulkTmdbMatchDialog::buildUi_()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    auto* intro = new QLabel(
        tr("Pick the best TMDb match for each title, or set it to skip. "
           "Nothing is changed until you press Assign."));
    intro->setWordWrap(true);
    root->addWidget(intro);

    // Progress row (search phase).
    auto* progRow = new QHBoxLayout;
    m_progress = new QProgressBar;
    m_progress->setRange(0, m_movies.size());
    m_progress->setValue(0);
    m_progressLbl = new QLabel;
    progRow->addWidget(m_progress, 1);
    progRow->addWidget(m_progressLbl);
    root->addLayout(progRow);

    // Table.
    m_table = new QTableWidget(m_movies.size(), ColCount);
    m_table->setHorizontalHeaderLabels(
        {QString(), tr("Title"), tr("TMDb match"), tr("Poster")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setColumnWidth(ColPick, 30);
    m_table->horizontalHeader()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColMatch, QHeaderView::Stretch);
    m_table->setColumnWidth(ColPoster, kPosterW + 12);
    m_table->verticalHeader()->setDefaultSectionSize(kPosterH + 8);

    for (int r = 0; r < m_movies.size(); ++r) {
        const Movie& m = m_movies[r];

        auto* pick = new QTableWidgetItem;
        pick->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        pick->setCheckState(Qt::Unchecked);   // enabled once candidates arrive
        m_table->setItem(r, ColPick, pick);

        QString titleText = m.title;
        if (m.productionYear > 0)
            titleText += QStringLiteral(" (%1)").arg(m.productionYear);
        if (m.tmdbId > 0)
            titleText += QStringLiteral("  ✓");   // already matched
        auto* titleItem = new QTableWidgetItem(titleText);
        titleItem->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(r, ColTitle, titleItem);

        auto* combo = new QComboBox;
        combo->addItem(tr("Searching…"));
        combo->setEnabled(false);
        m_table->setCellWidget(r, ColMatch, combo);
        connect(combo, &QComboBox::currentIndexChanged, this,
                [this, r](int) { loadPosterForRow_(r); });

        auto* poster = new QLabel;
        poster->setFixedSize(kPosterW, kPosterH);
        poster->setAlignment(Qt::AlignCenter);
        m_table->setCellWidget(r, ColPoster, poster);
    }
    root->addWidget(m_table, 1);

    // Footer.
    auto* footer = new QHBoxLayout;
    m_posterCheck = new QCheckBox(tr("Download posters from TMDb"));
    m_posterCheck->setChecked(true);   // default ON
    footer->addWidget(m_posterCheck);
    footer->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
    m_assignBtn = buttons->addButton(tr("Assign"), QDialogButtonBox::AcceptRole);
    m_assignBtn->setObjectName(QStringLiteral("primary"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    footer->addWidget(buttons);
    root->addLayout(footer);

    updateProgress_();
}

void BulkTmdbMatchDialog::startSearches_()
{
    if (!m_tmdb) return;
    connect(m_tmdb, &TmdbClient::searchForFinished,
            this, &BulkTmdbMatchDialog::onSearchResult_);
    pumpSearchQueue_();
}

void BulkTmdbMatchDialog::pumpSearchQueue_()
{
    while (m_inFlight < kMaxInFlight && m_nextToStart < m_movies.size()) {
        const int row = m_nextToStart++;
        const Movie& m = m_movies[row];
        ++m_inFlight;
        m_tmdb->searchFor(static_cast<quint64>(row), m.title, m.productionYear);
    }
}

void BulkTmdbMatchDialog::onSearchResult_(quint64 requestId,
                                          const QList<TmdbCandidate>& candidates,
                                          const QString& error)
{
    const int row = static_cast<int>(requestId);
    if (row < 0 || row >= m_movies.size()) return;

    --m_inFlight;
    ++m_completed;
    if (error.isEmpty())
        populateRow_(row, candidates);
    else
        populateRow_(row, {});   // treat as "no match"

    updateProgress_();
    pumpSearchQueue_();
}

void BulkTmdbMatchDialog::populateRow_(int row, const QList<TmdbCandidate>& candidates)
{
    m_rowCandidates.insert(row, candidates);

    auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColMatch));
    if (!combo) return;
    combo->clear();

    if (candidates.isEmpty()) {
        combo->addItem(tr("No match"));
        combo->setEnabled(false);
        if (auto* pick = m_table->item(row, ColPick))
            pick->setCheckState(Qt::Unchecked);
        loadPosterForRow_(row);
        return;
    }

    combo->setEnabled(true);
    combo->addItem(tr("— Skip —"), 0);   // tmdbId 0 = skip

    // Best guess: prefer a candidate whose year matches the movie's, else first.
    const int wantYear = m_movies[row].productionYear;
    int bestIndex = 1;   // first real candidate (after Skip)
    bool yearMatched = false;
    for (int i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        QString label = c.title;
        if (c.year() > 0) label += QStringLiteral(" (%1)").arg(c.year());
        combo->addItem(label, c.id);
        combo->setItemData(combo->count() - 1, c.posterPath, kRolePoster);
        combo->setItemData(combo->count() - 1, c.id, kRoleTmdbId);
        if (!yearMatched && wantYear > 0 && c.year() == wantYear) {
            bestIndex = combo->count() - 1;
            yearMatched = true;
        }
    }
    combo->setCurrentIndex(bestIndex);

    if (auto* pick = m_table->item(row, ColPick))
        pick->setCheckState(Qt::Checked);   // pre-check rows with a candidate

    loadPosterForRow_(row);
}

void BulkTmdbMatchDialog::loadPosterForRow_(int row)
{
    auto* combo  = qobject_cast<QComboBox*>(m_table->cellWidget(row, ColMatch));
    auto* poster = qobject_cast<QLabel*>(m_table->cellWidget(row, ColPoster));
    if (!combo || !poster) return;

    const QString posterPath = combo->currentData(kRolePoster).toString();
    QPixmap placeholder(kPosterW, kPosterH);
    placeholder.fill(QColor(70, 70, 70));
    poster->setPixmap(placeholder);

    if (posterPath.isEmpty() || !m_tmdb) return;
    const QString url = m_tmdb->imageUrl(posterPath, QStringLiteral("w92"));
    if (url.isEmpty()) return;
    QNetworkAccessManager* nam = m_tmdb->network();
    if (!nam) return;

    QPointer<QLabel> guard(poster);   // row widget may outlive request; still safe
    auto* reply = nam->get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, this, [reply, guard]() {
        reply->deleteLater();
        if (!guard || reply->error() != QNetworkReply::NoError) return;
        QPixmap pm;
        pm.loadFromData(reply->readAll());
        if (!pm.isNull())
            guard->setPixmap(pm.scaled(kPosterW, kPosterH, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
    });
}

void BulkTmdbMatchDialog::updateProgress_()
{
    const int total = m_movies.size();
    m_progress->setValue(m_completed);
    if (m_completed >= total) {
        m_progressLbl->setText(tr("Search complete"));
        m_progress->setVisible(false);
        m_progressLbl->setVisible(false);
    } else {
        m_progressLbl->setText(tr("Searching %1 / %2…").arg(m_completed).arg(total));
    }
}

QList<TmdbBulkMatch> BulkTmdbMatchDialog::matches() const
{
    QList<TmdbBulkMatch> out;
    for (int r = 0; r < m_movies.size(); ++r) {
        const auto* pick = m_table->item(r, ColPick);
        if (!pick || pick->checkState() != Qt::Checked) continue;
        const auto* combo = qobject_cast<QComboBox*>(m_table->cellWidget(r, ColMatch));
        if (!combo || !combo->isEnabled()) continue;
        const int tmdbId = combo->currentData(kRoleTmdbId).toInt();
        if (tmdbId <= 0) continue;   // "Skip" selected
        out.append({ m_movies[r].id, tmdbId,
                     combo->currentData(kRolePoster).toString() });
    }
    return out;
}

bool BulkTmdbMatchDialog::downloadPosters() const
{
    return m_posterCheck && m_posterCheck->isChecked();
}

} // namespace xyz
