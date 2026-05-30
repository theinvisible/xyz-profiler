#include "ui/MovieDetailWidget.h"

#include "ui/CoverArt.h"
#include "ui/CoverCache.h"
#include "ui/FlowLayout.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QDate>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace xyz {

// ---------------------------------------------------------------------------
// StarBar — paints a 0..5 star rating (xp-stars). No Q_OBJECT (paint-only).
// ---------------------------------------------------------------------------
class StarBar : public QWidget {
public:
    explicit StarBar(int px = 16, QWidget* parent = nullptr)
        : QWidget(parent), m_px(px) {}

    void setValue(int v) { m_value = qBound(0, v, 5); update(); }

    QSize sizeHint() const override
    {
        return { 5 * m_px + 4 * m_gap, m_px };
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal dpr = devicePixelRatioF();
        int x = 0;
        for (int i = 0; i < 5; ++i) {
            const QColor c = (i < m_value) ? Theme::starOn() : Theme::starOff();
            p.drawPixmap(x, 0, IconFactory::pixmap(QStringLiteral("star"), c,
                                                   m_px, 1.0, dpr));
            x += m_px + m_gap;
        }
    }

private:
    int m_px;
    int m_gap = 2;
    int m_value = 0;
};

namespace {

void clearLayout(QLayout* layout)
{
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        if (QLayout* child = item->layout()) clearLayout(child);
        delete item;
    }
}

QLabel* captionLabel(const QString& text)
{
    auto* l = new QLabel(text.toUpper());
    QFont f = l->font();
    f.setPointSizeF(8.0);
    f.setBold(true);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text3.name()));
    return l;
}

QLabel* valueLabel(const QString& text)
{
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text.name()));
    return l;
}

QWidget* makeField(const QString& label, const QString& value)
{
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(3);
    v->addWidget(captionLabel(label));
    v->addWidget(valueLabel(value));
    return w;
}

QLabel* subhead(const QString& text)
{
    auto* l = new QLabel(text.toUpper());
    QFont f = l->font();
    f.setPointSizeF(8.5);
    f.setBold(true);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text3.name()));
    return l;
}

QLabel* chip(const QString& text)
{
    const Palette& p = Theme::current();
    auto* l = new QLabel(text);
    l->setStyleSheet(QStringLiteral(
        "QLabel{background:%1;color:%2;border:1px solid %3;"
        "border-radius:11px;padding:3px 10px;}")
        .arg(p.panel3.name(), p.text2.name(), p.border.name()));
    return l;
}

QLabel* dotSep()
{
    auto* l = new QLabel(QStringLiteral("·"));
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text3.name()));
    return l;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
MovieDetailWidget::MovieDetailWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("detailPane"));
    // Resizable via the splitter — keep a sane minimum so the cover + text
    // column never collapse. The default width is set through the splitter
    // sizes / persisted splitter state in MainWindow.
    setMinimumWidth(360);
    buildUi_();
    clearSelection();
}

void MovieDetailWidget::buildUi_()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget;
    root->addWidget(m_stack);

    // --- Placeholder (page 0) ---------------------------------------------
    {
        auto* ph = new QWidget;
        auto* v  = new QVBoxLayout(ph);
        v->setAlignment(Qt::AlignCenter);
        v->setSpacing(16);

        auto* icon = new QLabel;
        icon->setPixmap(IconFactory::pixmap(QStringLiteral("film"),
                                            Theme::current().text3, 46, 1.1,
                                            devicePixelRatioF()));
        icon->setAlignment(Qt::AlignCenter);
        v->addWidget(icon);

        auto* text = new QLabel(tr("Select a movie from the collection\nto see its details."));
        text->setAlignment(Qt::AlignCenter);
        text->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text3.name()));
        v->addWidget(text);

        m_stack->addWidget(ph);
    }

    // --- Content (page 1) -------------------------------------------------
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("detailInner"));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    buildHeader_(cl);

    // Loan banner.
    m_loanBanner = new QFrame;
    auto* lb = new QHBoxLayout(m_loanBanner);
    lb->setContentsMargins(12, 8, 12, 8);
    lb->setSpacing(8);
    m_loanIcon = new QLabel;
    m_loanText = new QLabel;
    lb->addWidget(m_loanIcon);
    lb->addWidget(m_loanText, 1);
    auto* bannerWrap = new QWidget;
    auto* bw = new QVBoxLayout(bannerWrap);
    bw->setContentsMargins(16, 0, 16, 5);
    bw->addWidget(m_loanBanner);
    cl->addWidget(bannerWrap);

    buildTabs_(cl);

    m_stack->addWidget(content);
}

void MovieDetailWidget::buildHeader_(QVBoxLayout* contentLayout)
{
    auto* head = new QWidget;
    auto* h = new QHBoxLayout(head);
    h->setContentsMargins(16, 12, 16, 10);
    h->setSpacing(14);

    m_cover = new QLabel;
    m_cover->setFixedSize(150, 225);
    m_cover->setAlignment(Qt::AlignCenter);
    h->addWidget(m_cover, 0, Qt::AlignTop);

    auto* main = new QVBoxLayout;
    main->setSpacing(0);

    m_title = new QLabel;
    m_title->setWordWrap(true);
    { QFont f = m_title->font(); f.setPointSize(17); f.setBold(true); m_title->setFont(f); }
    main->addWidget(m_title);

    m_original = new QLabel;
    m_original->setWordWrap(true);
    { QFont f = m_original->font(); f.setItalic(true); m_original->setFont(f); }
    main->addSpacing(3);
    main->addWidget(m_original);

    m_metaRow = new QWidget;
    m_metaLayout = new QHBoxLayout(m_metaRow);
    m_metaLayout->setContentsMargins(0, 0, 0, 0);
    m_metaLayout->setSpacing(8);
    main->addSpacing(10);
    main->addWidget(m_metaRow);

    m_chips = new QWidget;
    new FlowLayout(m_chips, 0, 6, 6);
    main->addSpacing(11);
    main->addWidget(m_chips);

    m_ratingRow = new QWidget;
    auto* rr = new QHBoxLayout(m_ratingRow);
    rr->setContentsMargins(0, 0, 0, 0);
    rr->setSpacing(10);
    auto* ratingBlock = new QVBoxLayout;
    ratingBlock->setSpacing(4);
    m_stars = new StarBar(16);
    ratingBlock->addWidget(m_stars);
    m_ratingCap = captionLabel(tr("My rating"));
    ratingBlock->addWidget(m_ratingCap);
    rr->addLayout(ratingBlock);
    rr->addStretch();
    main->addSpacing(16);
    main->addWidget(m_ratingRow);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_tmdbBtn = new QPushButton(tr("Find on TMDb…"));
    m_tmdbBtn->setObjectName(QStringLiteral("primary"));
    m_tmdbBtn->setCursor(Qt::PointingHandCursor);
    connect(m_tmdbBtn, &QPushButton::clicked, this, &MovieDetailWidget::tmdbSearchRequested);
    actions->addWidget(m_tmdbBtn);
    actions->addStretch();
    main->addSpacing(18);
    main->addLayout(actions);
    main->addStretch();

    h->addLayout(main, 1);
    contentLayout->addWidget(head);
}

void MovieDetailWidget::buildTabs_(QVBoxLayout* contentLayout)
{
    m_tabs = new QTabWidget;
    m_tabs->setDocumentMode(true);

    const auto makePage = [](QVBoxLayout*& outLayout) -> QScrollArea* {
        auto* area = new QScrollArea;
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* page = new QWidget;
        outLayout = new QVBoxLayout(page);
        outLayout->setContentsMargins(16, 12, 16, 16);
        outLayout->setSpacing(0);
        area->setWidget(page);
        return area;
    };

    // --- Overview ---------------------------------------------------------
    {
        QVBoxLayout* l = nullptr;
        auto* area = makePage(l);
        m_overviewText = new QLabel;
        m_overviewText->setWordWrap(true);
        m_overviewText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        { QFont f = m_overviewText->font(); f.setPointSizeF(f.pointSizeF() + 0.5); m_overviewText->setFont(f); }
        l->addWidget(m_overviewText);

        auto* facts = new QWidget;
        m_factsGrid = new QGridLayout(facts);
        m_factsGrid->setContentsMargins(0, 0, 0, 0);
        m_factsGrid->setHorizontalSpacing(22);
        m_factsGrid->setVerticalSpacing(16);
        m_factsGrid->setColumnStretch(0, 1);
        m_factsGrid->setColumnStretch(1, 1);
        l->addSpacing(22);
        l->addWidget(facts);

        m_bonusSection = new QWidget;
        auto* bs = new QVBoxLayout(m_bonusSection);
        bs->setContentsMargins(0, 0, 0, 0);
        bs->setSpacing(9);
        bs->addWidget(subhead(tr("Bonus features")));
        auto* bonusItems = new QWidget;
        m_bonusLayout = new QVBoxLayout(bonusItems);
        m_bonusLayout->setContentsMargins(0, 0, 0, 0);
        m_bonusLayout->setSpacing(9);
        bs->addWidget(bonusItems);
        l->addSpacing(8);
        l->addWidget(m_bonusSection);

        l->addStretch();
        m_tabs->addTab(area, tr("Overview"));
    }

    // --- Cast & Crew ------------------------------------------------------
    {
        QVBoxLayout* l = nullptr;
        auto* area = makePage(l);

        m_castSection = new QWidget;
        auto* cs = new QVBoxLayout(m_castSection);
        cs->setContentsMargins(0, 0, 0, 0);
        cs->setSpacing(12);
        cs->addWidget(subhead(tr("Cast")));
        auto* castItems = new QWidget;
        m_castLayout = new QVBoxLayout(castItems);
        m_castLayout->setContentsMargins(0, 0, 0, 0);
        m_castLayout->setSpacing(13);
        cs->addWidget(castItems);
        l->addWidget(m_castSection);

        m_crewSection = new QWidget;
        auto* cw = new QVBoxLayout(m_crewSection);
        cw->setContentsMargins(0, 0, 0, 0);
        cw->setSpacing(0);
        cw->addWidget(subhead(tr("Crew")));
        auto* crewItems = new QWidget;
        m_crewLayout = new QVBoxLayout(crewItems);
        m_crewLayout->setContentsMargins(0, 0, 0, 0);
        m_crewLayout->setSpacing(0);
        cw->addWidget(crewItems);
        l->addSpacing(24);
        l->addWidget(m_crewSection);

        l->addStretch();
        m_tabs->addTab(area, tr("Cast && Crew"));
    }

    // --- Tech -------------------------------------------------------------
    {
        QVBoxLayout* l = nullptr;
        auto* area = makePage(l);
        auto* spec = new QWidget;
        m_specGrid = new QGridLayout(spec);
        m_specGrid->setContentsMargins(0, 0, 0, 0);
        m_specGrid->setHorizontalSpacing(22);
        m_specGrid->setVerticalSpacing(18);
        m_specGrid->setColumnStretch(0, 1);
        m_specGrid->setColumnStretch(1, 1);
        l->addWidget(spec);
        l->addStretch();
        m_tabs->addTab(area, tr("Tech"));
    }

    // --- Notes ------------------------------------------------------------
    {
        QVBoxLayout* l = nullptr;
        auto* area = makePage(l);
        m_notesText = new QLabel;
        m_notesText->setWordWrap(true);
        m_notesText->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->addWidget(m_notesText);
        auto* extra = new QWidget;
        m_notesGrid = new QGridLayout(extra);
        m_notesGrid->setContentsMargins(0, 0, 0, 0);
        m_notesGrid->setHorizontalSpacing(22);
        m_notesGrid->setVerticalSpacing(16);
        m_notesGrid->setColumnStretch(0, 1);
        m_notesGrid->setColumnStretch(1, 1);
        l->addSpacing(20);
        l->addWidget(extra);
        l->addStretch();
        m_tabs->addTab(area, tr("Notes"));
    }

    // Fill a tab the first time it becomes visible for the current movie.
    connect(m_tabs, &QTabWidget::currentChanged, this,
            [this](int index) { populateTab_(index); });

    contentLayout->addWidget(m_tabs, 1);
}

// ---------------------------------------------------------------------------
// Population
// ---------------------------------------------------------------------------
void MovieDetailWidget::clearSelection()
{
    m_hasMovie = false;
    m_stack->setCurrentIndex(0);
}

void MovieDetailWidget::refreshTheme()
{
    if (m_hasMovie) updateFromMovie(m_current);
}

void MovieDetailWidget::updateFromMovie(const Movie& movie)
{
    m_current = movie;
    m_hasMovie = true;
    m_stack->setCurrentIndex(1);

    setUpdatesEnabled(false);
    populateHeader_(movie);

    // Lazily populate tabs: only rebuild the one the user is looking at; the
    // others are marked stale and filled when switched to. Rebuilding all four
    // (esp. the per-actor Cast & Crew widgets) on every selection change is
    // what made cursor navigation feel sluggish. The current tab is kept
    // across movies rather than reset to Overview.
    for (bool& done : m_tabPopulated) done = false;
    populateTab_(m_tabs ? m_tabs->currentIndex() : TabOverview);
    setUpdatesEnabled(true);
}

void MovieDetailWidget::populateTab_(int index)
{
    if (index < 0 || index >= TabCount || m_tabPopulated[index]) return;
    switch (index) {
    case TabOverview: populateOverview_(m_current); break;
    case TabCast:     populateCast_(m_current);     break;
    case TabTech:     populateTech_(m_current);     break;
    case TabNotes:    populateNotes_(m_current);    break;
    default: return;
    }
    m_tabPopulated[index] = true;
}

void MovieDetailWidget::populateHeader_(const Movie& m)
{
    const Palette& pal = Theme::current();
    const qreal dpr = devicePixelRatioF();

    // Cover.
    QPixmap cover;
    if (!m.coverFrontPath.isEmpty()) {
        const QString key = CoverCache::key(m.coverFrontPath, QStringLiteral("detail"));
        if (!QPixmapCache::find(key, &cover)) {
            QPixmap raw(m.coverFrontPath);
            if (!raw.isNull()) {
                cover = raw.scaled(QSize(150, 225) * dpr, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
                cover.setDevicePixelRatio(dpr);
                QPixmapCache::insert(key, cover);
            }
        }
    }
    if (cover.isNull())
        cover = CoverArt::placeholder(m.title, m.productionYear, m.format,
                                      QSize(150, 225), true, dpr);
    m_cover->setPixmap(cover);

    // Title / original.
    m_title->setText(m.title);
    const bool hasOrig = !m.originalTitle.isEmpty() && m.originalTitle != m.title;
    m_original->setVisible(hasOrig);
    if (hasOrig) {
        m_original->setText(m.originalTitle);
        m_original->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
    }

    // Meta line.
    clearLayout(m_metaLayout);
    const auto addMeta = [&](QWidget* w) {
        if (m_metaLayout->count() > 0) m_metaLayout->addWidget(dotSep());
        m_metaLayout->addWidget(w);
    };
    if (m.productionYear > 0) {
        auto* l = new QLabel(QString::number(m.productionYear));
        l->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
        addMeta(l);
    }
    if (m.runningTimeMinutes > 0) {
        const int h = m.runningTimeMinutes / 60, mi = m.runningTimeMinutes % 60;
        const QString rt = h > 0 ? tr("%1 h %2 min").arg(h).arg(mi)
                                 : tr("%1 min").arg(mi);
        auto* l = new QLabel(rt);
        l->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
        addMeta(l);
    }
    if (m.rating.age > 0 || !m.rating.value.isEmpty()) {
        const QString sys = m.rating.system.isEmpty() ? QStringLiteral("FSK") : m.rating.system;
        const QString val = m.rating.value.isEmpty() ? QString::number(m.rating.age) : m.rating.value;
        auto* badge = new QLabel(QStringLiteral("%1 %2").arg(sys, val));
        QFont f = badge->font(); f.setBold(true); f.setPointSizeF(f.pointSizeF() - 1); badge->setFont(f);
        badge->setStyleSheet(QStringLiteral(
            "QLabel{background:%1;color:white;border-radius:4px;padding:1px 7px;}")
            .arg(Theme::ageColor(m.rating.age).name()));
        addMeta(badge);
    }
    if (!m.countriesOfOrigin.isEmpty()) {
        auto* l = new QLabel(m.countriesOfOrigin.first());
        l->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
        addMeta(l);
    }
    m_metaRow->setVisible(m_metaLayout->count() > 0);

    // Genre chips.
    clearLayout(m_chips->layout());
    for (const QString& g : m.genres)
        m_chips->layout()->addWidget(chip(g));
    m_chips->setVisible(!m.genres.isEmpty());

    // Rating.
    const bool hasRating = m.review.film > 0;
    m_ratingRow->setVisible(hasRating);
    if (hasRating) {
        m_stars->setValue(qRound(m.review.film / 2.0));
        m_ratingCap->setText(tr("My rating · %1/10").arg(m.review.film).toUpper());
        m_ratingCap->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text3.name()));
    }

    // TMDb button.
    m_tmdbBtn->setText(m.tmdbId > 0 ? tr("Re-match on TMDb…") : tr("Find on TMDb…"));

    // Loan banner.
    if (m.loan.loaned) {
        m_loanBanner->parentWidget()->setVisible(true);
        m_loanBanner->setVisible(true);
        const QColor accent = Theme::loanAccent();
        m_loanBanner->setStyleSheet(QStringLiteral(
            "QFrame{background:rgba(%1,%2,%3,0.14);border:1px solid rgba(%1,%2,%3,0.4);"
            "border-radius:6px;}")
            .arg(accent.red()).arg(accent.green()).arg(accent.blue()));
        m_loanIcon->setPixmap(IconFactory::pixmap(QStringLiteral("user"), accent, 14, 1.6, dpr));
        QString name = QStringLiteral("%1 %2").arg(m.loan.userFirstName, m.loan.userLastName).trimmed();
        QString text = name.isEmpty() ? tr("Loaned out") : tr("Loaned to %1").arg(name);
        if (m.loan.due.isValid())
            text += tr(" · due %1").arg(m.loan.due.toString(QStringLiteral("dd MMM yyyy")));
        m_loanText->setText(text);
        m_loanText->setStyleSheet(QStringLiteral("color:%1;font-weight:500;")
            .arg(Theme::isDark() ? QStringLiteral("#e9a94e") : QStringLiteral("#c47b1e")));
    } else {
        m_loanBanner->parentWidget()->setVisible(false);
    }
}

void MovieDetailWidget::populateOverview_(const Movie& m)
{
    m_overviewText->setVisible(!m.overview.isEmpty());
    m_overviewText->setText(m.overview);
    m_overviewText->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text.name()));

    clearLayout(m_factsGrid);
    int idx = 0;
    const auto addFact = [&](const QString& label, const QString& value) {
        if (value.isEmpty()) return;
        m_factsGrid->addWidget(makeField(label, value), idx / 2, idx % 2);
        ++idx;
    };
    addFact(tr("Studio"), m.studios.join(QStringLiteral(", ")));
    addFact(tr("Format"), Theme::formatBadge(m.format).label);
    addFact(tr("Location"), m.locationId);
    QDate added = m.purchase.date.isValid() ? m.purchase.date
                                            : m.profileTimestamp.date();
    addFact(tr("Added"), added.isValid()
                ? added.toString(QStringLiteral("dd MMMM yyyy")) : QString());

    // Bonus features.
    clearLayout(m_bonusLayout);
    QStringList bonus = m.features;
    if (!m.otherFeatures.isEmpty()) bonus << m.otherFeatures;
    for (const QString& b : bonus) {
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(9);
        auto* icon = new QLabel;
        icon->setPixmap(IconFactory::pixmap(QStringLiteral("check"),
                                            Theme::current().accent, 14, 1.8,
                                            devicePixelRatioF()));
        h->addWidget(icon, 0, Qt::AlignTop);
        h->addWidget(valueLabel(b), 1);
        m_bonusLayout->addWidget(row);
    }
    m_bonusSection->setVisible(!bonus.isEmpty());
}

void MovieDetailWidget::populateCast_(const Movie& m)
{
    const Palette& pal = Theme::current();

    clearLayout(m_castLayout);
    for (const auto& a : m.actors) {
        const QString name = QStringLiteral("%1 %2").arg(a.firstName, a.lastName).trimmed();
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(12);

        // Avatar.
        QString initials;
        for (const QString& part : name.split(QChar(u' '), Qt::SkipEmptyParts)) {
            if (!part.isEmpty()) initials += part.at(0);
            if (initials.size() == 2) break;
        }
        auto* av = new QLabel(initials.toUpper());
        av->setFixedSize(38, 38);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet(QStringLiteral(
            "QLabel{background:%1;color:white;border-radius:19px;font-weight:600;}")
            .arg(Theme::avatarColor(name).name()));
        h->addWidget(av);

        auto* info = new QVBoxLayout;
        info->setSpacing(1);
        auto* nm = new QLabel(name);
        { QFont f = nm->font(); f.setBold(true); nm->setFont(f); }
        nm->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text.name()));
        info->addWidget(nm);
        if (!a.role.isEmpty()) {
            auto* rl = new QLabel(a.role);
            rl->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
            info->addWidget(rl);
        }
        h->addLayout(info, 1);
        m_castLayout->addWidget(row);
    }
    m_castSection->setVisible(!m.actors.isEmpty());

    clearLayout(m_crewLayout);
    for (const auto& c : m.credits) {
        const QString name = QStringLiteral("%1 %2").arg(c.firstName, c.lastName).trimmed();
        QString job = c.creditType;
        if (!c.role.isEmpty() && c.role != c.creditType)
            job = job.isEmpty() ? c.role : job + QStringLiteral(" / ") + c.role;

        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 9, 0, 9);
        auto* jl = new QLabel(job);
        jl->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
        auto* nl = new QLabel(name);
        { QFont f = nl->font(); f.setWeight(QFont::Medium); nl->setFont(f); }
        nl->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text.name()));
        nl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(jl);
        h->addStretch();
        h->addWidget(nl);

        // Hairline separator.
        row->setStyleSheet(QStringLiteral("border-bottom:1px solid %1;").arg(pal.border.name()));
        m_crewLayout->addWidget(row);
    }
    m_crewSection->setVisible(!m.credits.isEmpty());
}

void MovieDetailWidget::populateTech_(const Movie& m)
{
    clearLayout(m_specGrid);
    int idx = 0;
    const auto addSpec = [&](const QString& label, const QString& value) {
        if (value.isEmpty()) return;
        m_specGrid->addWidget(makeField(label, value), idx / 2, idx % 2);
        ++idx;
    };

    QStringList video;
    if (!m.videoFormat.videoStandard.isEmpty()) video << m.videoFormat.videoStandard;
    if (!m.videoFormat.dimensions.isEmpty())    video << m.videoFormat.dimensions;
    addSpec(tr("Video"), video.join(QStringLiteral(" · ")));
    addSpec(tr("Aspect ratio"), m.videoFormat.aspectRatio);

    QStringList audio;
    for (const auto& t : m.audioTracks) {
        QStringList parts;
        if (!t.content.isEmpty())  parts << t.content;
        if (!t.format.isEmpty())   parts << t.format;
        if (!t.channels.isEmpty()) parts << t.channels;
        audio << parts.join(QStringLiteral(" · "));
    }
    addSpec(tr("Audio"), audio.join(QChar(u'\n')));
    addSpec(tr("Subtitles"), m.subtitles.join(QStringLiteral(", ")));
    addSpec(tr("Regions"), m.regions.join(QStringLiteral(", ")));
    if (!m.discs.isEmpty())
        addSpec(tr("Discs"), QStringLiteral("%1 × %2")
                    .arg(m.discs.size()).arg(Theme::formatBadge(m.format).label));
}

void MovieDetailWidget::populateNotes_(const Movie& m)
{
    QString notes = m.notes;
    if (!m.easterEggs.isEmpty()) {
        if (!notes.isEmpty()) notes += QStringLiteral("\n\n");
        notes += tr("Easter eggs: ") + m.easterEggs;
    }
    if (notes.isEmpty())
        notes = tr("No notes for this title yet.");
    m_notesText->setText(notes);
    m_notesText->setStyleSheet(QStringLiteral("color:%1;").arg(
        m.notes.isEmpty() && m.easterEggs.isEmpty()
            ? Theme::current().text3.name() : Theme::current().text.name()));

    clearLayout(m_notesGrid);
    int idx = 0;
    const auto addFact = [&](const QString& label, const QString& value) {
        if (value.isEmpty()) return;
        m_notesGrid->addWidget(makeField(label, value), idx / 2, idx % 2);
        ++idx;
    };
    QStringList purchase;
    if (m.purchase.date.isValid())
        purchase << m.purchase.date.toString(QStringLiteral("dd MMM yyyy"));
    if (!m.purchase.price.formattedValue.isEmpty())
        purchase << m.purchase.price.formattedValue;
    if (!m.purchase.place.isEmpty())
        purchase << m.purchase.place;
    addFact(tr("Purchase"), purchase.join(QStringLiteral(" · ")));
    addFact(tr("Tags"), m.tags.join(QStringLiteral(", ")));
}

} // namespace xyz
