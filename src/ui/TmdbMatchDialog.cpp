#include "TmdbMatchDialog.h"

#include <QFont>
#include <QListWidgetItem>
#include <QPixmap>
#include <QSvgRenderer>

namespace xyz {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TmdbMatchDialog::TmdbMatchDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("TMDb Match"));
    setModal(true);
    setMaximumSize(720, 640);
    resize(720, 640);
    buildUi();
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

void TmdbMatchDialog::setCandidates(
    const QList<TmdbCandidate>& candidates,
    const std::function<QString(const QString&, const QString&)>& imageUrlFn)
{
    m_listWidget->clear();
    m_posterLabels.clear();
    m_posterUrls.clear();

    for (int i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];

        auto* itemWidget = buildCandidateWidget(c, i);

        auto* item = new QListWidgetItem(m_listWidget);
        item->setData(Qt::UserRole, c.id);
        item->setSizeHint(itemWidget->sizeHint());
        m_listWidget->setItemWidget(item, itemWidget);

        // Build full poster URL for later async loading.
        if (!c.posterPath.isEmpty() && imageUrlFn) {
            m_posterUrls.append(imageUrlFn(c.posterPath, QStringLiteral("w185")));
        } else {
            m_posterUrls.append(QString());
        }
    }

    m_listWidget->setVisible(!candidates.isEmpty());
    m_searchingLabel->setVisible(false);
    m_errorLabel->setVisible(false);
}

void TmdbMatchDialog::setSearching(bool searching)
{
    m_searchingLabel->setVisible(searching);
    if (searching) {
        m_errorLabel->setVisible(false);
        m_listWidget->setVisible(false);
    }
}

void TmdbMatchDialog::setError(const QString& error)
{
    m_errorLabel->setText(error);
    m_errorLabel->setVisible(!error.isEmpty());
    m_searchingLabel->setVisible(false);
}

void TmdbMatchDialog::loadPosters(QNetworkAccessManager* nam)
{
    if (!nam) return;

    for (int i = 0; i < m_posterUrls.size(); ++i) {
        const auto& url = m_posterUrls.at(i);
        if (url.isEmpty() || i >= m_posterLabels.size()) continue;

        QLabel* label = m_posterLabels.at(i);
        auto* reply = nam->get(QNetworkRequest(QUrl(url)));

        connect(reply, &QNetworkReply::finished, this, [reply, label]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;

            QPixmap poster;
            poster.loadFromData(reply->readAll());
            if (!poster.isNull()) {
                label->setPixmap(poster.scaled(74, 110, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
            }
        });
    }
}

// -----------------------------------------------------------------------
// UI construction
// -----------------------------------------------------------------------

void TmdbMatchDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    // "Searching..." label
    m_searchingLabel = new QLabel(QStringLiteral("Searching TMDb…"), this);
    m_searchingLabel->setAlignment(Qt::AlignCenter);
    m_searchingLabel->setVisible(false);
    root->addWidget(m_searchingLabel);

    // Error label (red)
    m_errorLabel = new QLabel(this);
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setStyleSheet(QStringLiteral("color: red;"));
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    // Candidate list
    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVisible(false);
    root->addWidget(m_listWidget, 1);

    connect(m_listWidget, &QListWidget::itemClicked, this,
            [this](QListWidgetItem* item) {
                m_selectedTmdbId = item->data(Qt::UserRole).toInt();
                accept();
            });

    // TMDb attribution row
    auto* attrRow = new QHBoxLayout();
    attrRow->setContentsMargins(0, 4, 0, 0);

    auto* tmdbLogo = new QLabel(this);
    {
        QSvgRenderer renderer(QStringLiteral(":/tmdb_logo.svg"));
        if (renderer.isValid()) {
            QPixmap pix(120, 20);
            pix.fill(Qt::transparent);
            QPainter p(&pix);
            renderer.render(&p);
            tmdbLogo->setPixmap(pix);
        } else {
            tmdbLogo->setText(QStringLiteral("TMDb"));
        }
    }
    tmdbLogo->setFixedHeight(20);
    attrRow->addWidget(tmdbLogo);

    auto* attrText = new QLabel(
        QStringLiteral("This product uses the TMDb API but is not endorsed "
                       "or certified by TMDb."),
        this);
    attrText->setWordWrap(true);
    QFont attrFont = attrText->font();
    attrFont.setPointSize(9);
    attrText->setFont(attrFont);
    attrRow->addWidget(attrText, 1);
    root->addLayout(attrRow);

    // Button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(m_buttonBox);
}

QWidget* TmdbMatchDialog::buildCandidateWidget(const TmdbCandidate& candidate,
                                               int /*index*/)
{
    auto* widget = new QWidget(m_listWidget);
    auto* hLayout = new QHBoxLayout(widget);
    hLayout->setContentsMargins(4, 4, 4, 4);

    // Poster placeholder
    auto* posterLabel = new QLabel(widget);
    posterLabel->setFixedSize(74, 110);
    posterLabel->setAlignment(Qt::AlignCenter);
    // Grey placeholder rectangle
    QPixmap placeholder(74, 110);
    placeholder.fill(QColor(80, 80, 80));
    posterLabel->setPixmap(placeholder);
    hLayout->addWidget(posterLabel, 0, Qt::AlignTop);
    m_posterLabels.append(posterLabel);

    // Text column
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    // Title + year (14pt bold)
    auto* titleLabel = new QLabel(widget);
    {
        QString titleText = candidate.title;
        const int year = candidate.year();
        if (year > 0) {
            titleText += QStringLiteral(" (%1)").arg(year);
        }
        titleLabel->setText(titleText);
    }
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);

    // Original title (italic, only if different)
    if (!candidate.originalTitle.isEmpty()
        && candidate.originalTitle != candidate.title) {
        auto* origLabel = new QLabel(candidate.originalTitle, widget);
        QFont origFont = origLabel->font();
        origFont.setItalic(true);
        origLabel->setFont(origFont);
        origLabel->setWordWrap(true);
        textLayout->addWidget(origLabel);
    }

    // Overview (12pt, max ~3 lines)
    if (!candidate.overview.isEmpty()) {
        auto* overviewLabel = new QLabel(candidate.overview, widget);
        QFont overviewFont = overviewLabel->font();
        overviewFont.setPointSize(12);
        overviewLabel->setFont(overviewFont);
        overviewLabel->setWordWrap(true);
        overviewLabel->setMaximumHeight(60);
        textLayout->addWidget(overviewLabel);
    }

    // Vote line: "★ 7.5 · 1234 votes · TMDb #12345"
    {
        const QString voteLine = QStringLiteral("★ %1 · %2 votes · TMDb #%3")
            .arg(candidate.voteAverage, 0, 'f', 1)
            .arg(candidate.voteCount)
            .arg(candidate.id);
        auto* voteLabel = new QLabel(voteLine, widget);
        QFont voteFont = voteLabel->font();
        voteFont.setPointSize(10);
        voteLabel->setFont(voteFont);
        textLayout->addWidget(voteLabel);
    }

    textLayout->addStretch();
    hLayout->addLayout(textLayout, 1);

    return widget;
}

} // namespace xyz
