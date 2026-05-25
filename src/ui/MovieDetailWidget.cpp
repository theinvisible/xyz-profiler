#include "ui/MovieDetailWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPixmapCache>
#include <QScrollBar>
#include <QVBoxLayout>

namespace xyz {

QLabel* MovieDetailWidget::makeHeader(const QString& text)
{
    auto* label = new QLabel(text);
    QFont font  = label->font();
    font.setPointSize(11);
    font.setBold(true);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: #3a7bd5;"));
    return label;
}

QLabel* MovieDetailWidget::makeBody(const QString& text)
{
    auto* label = new QLabel(text);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

struct SectionWidgets {
    QWidget* section;
    QLabel*  header;
    QLabel*  body;
};

static SectionWidgets makeSectionWithBody(
    const QString& headerText,
    QVBoxLayout* parentLayout)
{
    auto* section = new QWidget;
    auto* layout  = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* header = new QLabel(headerText);
    {
        QFont f = header->font();
        f.setPointSize(11);
        f.setBold(true);
        header->setFont(f);
        header->setStyleSheet(QStringLiteral("color: #3a7bd5;"));
    }

    auto* body = new QLabel;
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);

    layout->addWidget(header);
    layout->addWidget(body);
    parentLayout->addWidget(section);

    return {section, header, body};
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MovieDetailWidget::MovieDetailWidget(QWidget* parent)
    : QScrollArea(parent)
{
    setFixedWidth(460);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);

    m_inner      = new QWidget;
    m_rootLayout = new QVBoxLayout(m_inner);
    m_rootLayout->setContentsMargins(16, 16, 16, 16);
    m_rootLayout->setSpacing(12);

    m_placeholder = new QLabel(QStringLiteral("Select a movie"));
    m_placeholder->setAlignment(Qt::AlignCenter);
    {
        QFont f = m_placeholder->font();
        f.setPointSize(14);
        f.setItalic(true);
        m_placeholder->setFont(f);
    }
    m_placeholder->setStyleSheet(QStringLiteral("color: #8b919e;"));
    m_rootLayout->addWidget(m_placeholder, 1, Qt::AlignCenter);

    m_content = new QWidget;
    auto* cl = new QVBoxLayout(m_content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(12);

    // 1. Title block
    m_titleLabel = new QLabel;
    { QFont f = m_titleLabel->font(); f.setPointSize(22); f.setBold(true); m_titleLabel->setFont(f); }
    m_titleLabel->setWordWrap(true);
    cl->addWidget(m_titleLabel);

    m_originalTitleLabel = new QLabel;
    { QFont f = m_originalTitleLabel->font(); f.setItalic(true); m_originalTitleLabel->setFont(f); }
    m_originalTitleLabel->setWordWrap(true);
    cl->addWidget(m_originalTitleLabel);

    m_distTraitLabel = new QLabel;
    { QFont f = m_distTraitLabel->font(); f.setPointSize(12); m_distTraitLabel->setFont(f); }
    m_distTraitLabel->setStyleSheet(QStringLiteral("color: #8b919e;"));
    cl->addWidget(m_distTraitLabel);

    m_metaLineLabel = new QLabel;
    m_metaLineLabel->setWordWrap(true);
    cl->addWidget(m_metaLineLabel);

    // 2. Cover
    m_coverLabel = new QLabel;
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setMaximumSize(240, 360);
    cl->addWidget(m_coverLabel, 0, Qt::AlignHCenter);

    // 3. TMDb row
    m_tmdbRow = new QWidget;
    auto* tmdbL = new QHBoxLayout(m_tmdbRow);
    tmdbL->setContentsMargins(0, 0, 0, 0);
    tmdbL->setSpacing(8);
    m_tmdbIdLabel = new QLabel;
    { QFont f = m_tmdbIdLabel->font(); f.setPointSize(12); m_tmdbIdLabel->setFont(f); }
    m_tmdbIdLabel->setStyleSheet(QStringLiteral("color: #3a7bd5;"));
    tmdbL->addWidget(m_tmdbIdLabel);
    m_tmdbButton = new QPushButton(QStringLiteral("Find on TMDb…"));
    tmdbL->addWidget(m_tmdbButton);
    tmdbL->addStretch();
    connect(m_tmdbButton, &QPushButton::clicked, this, &MovieDetailWidget::tmdbSearchRequested);
    cl->addWidget(m_tmdbRow);

    // 4. Loan badge
    m_loanBadge = new QFrame;
    m_loanBadge->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #8b2020; border-radius: 6px; padding: 8px; }"));
    auto* loanL = new QVBoxLayout(m_loanBadge);
    loanL->setContentsMargins(12, 8, 12, 8);
    loanL->setSpacing(2);
    m_loanTitleLabel = new QLabel(QStringLiteral("Loaned out"));
    { QFont f = m_loanTitleLabel->font(); f.setBold(true); m_loanTitleLabel->setFont(f); }
    m_loanTitleLabel->setStyleSheet(QStringLiteral("color: white;"));
    loanL->addWidget(m_loanTitleLabel);
    m_loanDetailLabel = new QLabel;
    m_loanDetailLabel->setStyleSheet(QStringLiteral("color: white;"));
    m_loanDetailLabel->setWordWrap(true);
    loanL->addWidget(m_loanDetailLabel);
    cl->addWidget(m_loanBadge);

    // Sections 5-17 — all static QLabel body (no dynamic widget creation)
    auto addSection = [&](const QString& h, QWidget*& sec, QLabel*& hdr, QLabel*& body) {
        auto sw = makeSectionWithBody(h, cl);
        sec = sw.section; hdr = sw.header; body = sw.body;
    };
    addSection(QStringLiteral("OVERVIEW"),    m_overviewSection,   m_overviewHeader,   m_overviewBody);
    addSection(QStringLiteral("NOTES"),       m_notesSection,      m_notesHeader,      m_notesBody);
    addSection(QStringLiteral("EASTER EGGS"), m_easterEggsSection, m_easterEggsHeader, m_easterEggsBody);
    addSection(QStringLiteral("GENRES"),      m_genresSection,     m_genresHeader,     m_genresBody);
    addSection(QStringLiteral("STUDIOS"),     m_studiosSection,    m_studiosHeader,    m_studiosBody);
    addSection(QStringLiteral("CAST"),        m_castSection,       m_castHeader,       m_castBody);
    addSection(QStringLiteral("CREW"),        m_crewSection,       m_crewHeader,       m_crewBody);
    addSection(QStringLiteral("AUDIO"),       m_audioSection,      m_audioHeader,      m_audioBody);
    addSection(QStringLiteral("SUBTITLES"),   m_subtitlesSection,  m_subtitlesHeader,  m_subtitlesBody);
    addSection(QStringLiteral("DISCS"),       m_discsSection,      m_discsHeader,      m_discsBody);
    addSection(QStringLiteral("TECHNICAL"),   m_technicalSection,  m_technicalHeader,  m_technicalBody);
    addSection(QStringLiteral("PURCHASE"),    m_purchaseSection,   m_purchaseHeader,   m_purchaseBody);
    addSection(QStringLiteral("TAGS"),        m_tagsSection,       m_tagsHeader,       m_tagsBody);

    cl->addStretch();
    m_rootLayout->addWidget(m_content);
    setWidget(m_inner);
    clearSelection();
}

void MovieDetailWidget::clearSelection()
{
    m_placeholder->setVisible(true);
    m_content->setVisible(false);
}

void MovieDetailWidget::updateFromMovie(const Movie& movie)
{
    setUpdatesEnabled(false);

    m_placeholder->setVisible(false);
    m_content->setVisible(true);

    // 1. Title block
    m_titleLabel->setText(movie.title);

    const bool hasOriginal = !movie.originalTitle.isEmpty() && movie.originalTitle != movie.title;
    m_originalTitleLabel->setVisible(hasOriginal);
    if (hasOriginal) m_originalTitleLabel->setText(movie.originalTitle);

    m_distTraitLabel->setVisible(!movie.distTrait.isEmpty());
    m_distTraitLabel->setText(movie.distTrait);

    QStringList metaParts;
    if (movie.productionYear > 0)    metaParts << QString::number(movie.productionYear);
    if (movie.runningTimeMinutes > 0) metaParts << QStringLiteral("%1 min").arg(movie.runningTimeMinutes);
    if (!movie.format.isEmpty())      metaParts << movie.format;
    if (!movie.rating.value.isEmpty()) metaParts << movie.rating.value;
    if (movie.rating.age > 0)         metaParts << QStringLiteral("age %1").arg(movie.rating.age);
    m_metaLineLabel->setText(metaParts.join(QStringLiteral(" · ")));

    // 2. Cover — cached via QPixmapCache
    if (!movie.coverFrontPath.isEmpty()) {
        if (movie.coverFrontPath == m_cachedCoverPath) {
            m_coverLabel->setVisible(true);
        } else {
            QPixmap pix;
            if (!QPixmapCache::find(movie.coverFrontPath, &pix)) {
                pix.load(movie.coverFrontPath);
                if (!pix.isNull()) {
                    pix = pix.scaled(240, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QPixmapCache::insert(movie.coverFrontPath, pix);
                }
            }
            if (!pix.isNull()) {
                m_coverLabel->setPixmap(pix);
                m_coverLabel->setVisible(true);
                m_cachedCoverPath = movie.coverFrontPath;
            } else {
                m_coverLabel->clear();
                m_coverLabel->setVisible(false);
                m_cachedCoverPath.clear();
            }
        }
    } else {
        m_coverLabel->clear();
        m_coverLabel->setVisible(false);
        m_cachedCoverPath.clear();
    }

    // 3. TMDb
    if (movie.tmdbId > 0) {
        m_tmdbIdLabel->setText(QStringLiteral("TMDb #%1").arg(movie.tmdbId));
        m_tmdbIdLabel->setVisible(true);
        m_tmdbButton->setText(QStringLiteral("Re-match on TMDb…"));
    } else {
        m_tmdbIdLabel->setVisible(false);
        m_tmdbButton->setText(QStringLiteral("Find on TMDb…"));
    }

    // 4. Loan badge
    if (movie.loan.loaned) {
        m_loanBadge->setVisible(true);
        const QString name = QStringLiteral("%1 %2")
            .arg(movie.loan.userFirstName, movie.loan.userLastName).trimmed();
        QString detail;
        if (!name.isEmpty()) detail = QStringLiteral("To %1").arg(name);
        if (movie.loan.due.isValid()) {
            if (!detail.isEmpty()) detail += QStringLiteral(", ");
            detail += QStringLiteral("due %1").arg(movie.loan.due.toString(Qt::ISODate));
        }
        m_loanDetailLabel->setText(detail);
    } else {
        m_loanBadge->setVisible(false);
    }

    // 5-9. Static text sections
    m_overviewSection->setVisible(!movie.overview.isEmpty());
    m_overviewBody->setText(movie.overview);
    m_notesSection->setVisible(!movie.notes.isEmpty());
    m_notesBody->setText(movie.notes);
    m_easterEggsSection->setVisible(!movie.easterEggs.isEmpty());
    m_easterEggsBody->setText(movie.easterEggs);
    m_genresSection->setVisible(!movie.genres.isEmpty());
    m_genresBody->setText(movie.genres.join(QStringLiteral(", ")));
    m_studiosSection->setVisible(!movie.studios.isEmpty());
    m_studiosBody->setText(movie.studios.join(QStringLiteral(", ")));

    // 10. Cast — single QLabel, newline-joined
    m_castSection->setVisible(!movie.actors.isEmpty());
    if (!movie.actors.isEmpty()) {
        m_castHeader->setText(QStringLiteral("CAST (%1)").arg(movie.actors.size()));
        QStringList lines;
        lines.reserve(movie.actors.size());
        for (const auto& p : movie.actors) {
            QString line = QStringLiteral("%1 %2").arg(p.firstName, p.lastName).trimmed();
            if (!p.role.isEmpty()) line += QStringLiteral("  —  %1").arg(p.role);
            lines << line;
        }
        m_castBody->setText(lines.join(QChar(u'\n')));
    }

    // 11. Crew
    m_crewSection->setVisible(!movie.credits.isEmpty());
    if (!movie.credits.isEmpty()) {
        m_crewHeader->setText(QStringLiteral("CREW (%1)").arg(movie.credits.size()));
        QStringList lines;
        lines.reserve(movie.credits.size());
        for (const auto& p : movie.credits) {
            QString line = QStringLiteral("%1 %2").arg(p.firstName, p.lastName).trimmed();
            QStringList qual;
            if (!p.creditType.isEmpty()) qual << p.creditType;
            if (!p.role.isEmpty() && p.role != p.creditType) qual << p.role;
            if (!qual.isEmpty()) line += QStringLiteral("  —  %1").arg(qual.join(QStringLiteral(" / ")));
            lines << line;
        }
        m_crewBody->setText(lines.join(QChar(u'\n')));
    }

    // 12. Audio
    m_audioSection->setVisible(!movie.audioTracks.isEmpty());
    if (!movie.audioTracks.isEmpty()) {
        m_audioHeader->setText(QStringLiteral("AUDIO (%1)").arg(movie.audioTracks.size()));
        QStringList lines;
        for (const auto& t : movie.audioTracks) {
            QStringList parts;
            if (!t.content.isEmpty()) parts << t.content;
            if (!t.format.isEmpty())  parts << t.format;
            if (!t.channels.isEmpty()) parts << t.channels;
            lines << parts.join(QStringLiteral(" · "));
        }
        m_audioBody->setText(lines.join(QChar(u'\n')));
    }

    // 13. Subtitles
    m_subtitlesSection->setVisible(!movie.subtitles.isEmpty());
    m_subtitlesBody->setText(movie.subtitles.join(QStringLiteral(", ")));

    // 14. Discs
    m_discsSection->setVisible(!movie.discs.isEmpty());
    if (!movie.discs.isEmpty()) {
        m_discsHeader->setText(QStringLiteral("DISCS (%1)").arg(movie.discs.size()));
        QStringList lines;
        for (const auto& d : movie.discs) {
            QStringList parts;
            if (!d.descriptionSideA.isEmpty()) parts << d.descriptionSideA;
            if (!d.discIdSideA.isEmpty()) parts << QStringLiteral("ID %1").arg(d.discIdSideA);
            if (!d.labelSideA.isEmpty()) parts << d.labelSideA;
            lines << parts.join(QStringLiteral(" · "));
        }
        m_discsBody->setText(lines.join(QChar(u'\n')));
    }

    // 15. Technical
    {
        QStringList lines;
        QStringList first;
        if (!movie.videoFormat.aspectRatio.isEmpty()) first << movie.videoFormat.aspectRatio;
        if (!movie.videoFormat.dimensions.isEmpty())  first << movie.videoFormat.dimensions;
        if (!movie.caseType.isEmpty())                first << movie.caseType;
        if (!first.isEmpty()) lines << first.join(QStringLiteral(" · "));
        if (!movie.regions.isEmpty())
            lines << QStringLiteral("Regions: %1").arg(movie.regions.join(QStringLiteral(", ")));
        if (!movie.features.isEmpty())
            lines << QStringLiteral("Features: %1").arg(movie.features.join(QStringLiteral(", ")));
        m_technicalSection->setVisible(!lines.isEmpty());
        m_technicalBody->setText(lines.join(QChar(u'\n')));
    }

    // 16. Purchase
    {
        QStringList parts;
        if (movie.purchase.date.isValid())
            parts << movie.purchase.date.toString(Qt::ISODate);
        if (!movie.purchase.price.formattedValue.isEmpty())
            parts << movie.purchase.price.formattedValue;
        else if (!movie.purchase.price.value.isEmpty() && movie.purchase.price.value != QStringLiteral("0"))
            parts << QStringLiteral("%1 %2").arg(movie.purchase.price.value, movie.purchase.price.denominationType);
        if (!movie.purchase.place.isEmpty())
            parts << movie.purchase.place;
        m_purchaseSection->setVisible(!parts.isEmpty());
        m_purchaseBody->setText(parts.join(QStringLiteral(", ")));
    }

    // 17. Tags
    m_tagsSection->setVisible(!movie.tags.isEmpty());
    m_tagsBody->setText(movie.tags.join(QStringLiteral(", ")));

    verticalScrollBar()->setValue(0);
    setUpdatesEnabled(true);
}

} // namespace xyz
