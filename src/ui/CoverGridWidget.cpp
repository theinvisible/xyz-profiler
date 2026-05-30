#include "ui/CoverGridWidget.h"

#include "models/MovieListModel.h"
#include "ui/CoverArt.h"
#include "ui/CoverCache.h"
#include "ui/Theme.h"

#include <QItemSelectionModel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QScrollBar>

namespace xyz {

// ---------------------------------------------------------------------------
// Layout constants (xp-card / xp-cover-grid)
// ---------------------------------------------------------------------------
namespace {

constexpr int kTileW   = 168;
constexpr int kTileH   = 300;
constexpr int kPad     = 6;     // card padding
constexpr int kCoverW  = kTileW - 2 * kPad;            // 156
constexpr int kCoverH  = int(kCoverW * 1.5);           // 234 (2:3)
constexpr int kMetaTop = kPad + kCoverH + 9;
constexpr int kCardRadius  = 9;
constexpr int kCoverRadius = 6;

QString firstGenre(const QModelIndex& index)
{
    const QString g = index.data(MovieListModel::GenresJoinedRole).toString();
    const int comma = g.indexOf(QChar(u','));
    return comma >= 0 ? g.left(comma).trimmed() : g;
}

// Cached placeholder poster — generating the gradient/perforation/text every
// paint is what makes scrolling stutter, so we render once and reuse.
QPixmap gridPlaceholder(const QString& title, int year,
                        const QString& format, qreal dpr)
{
    const QString key = QStringLiteral("xpgrid_ph_%1_%2_%3_%4")
        .arg(title, QString::number(year), format, QString::number(dpr));
    QPixmap pm;
    if (!QPixmapCache::find(key, &pm)) {
        pm = CoverArt::placeholder(title, year, format,
                                   QSize(kCoverW, kCoverH), true, dpr);
        QPixmapCache::insert(key, pm);
    }
    return pm;
}

} // namespace

// ---------------------------------------------------------------------------
// CoverDelegate
// ---------------------------------------------------------------------------
CoverDelegate::CoverDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QSize CoverDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return {kTileW, kTileH};
}

void CoverDelegate::paint(QPainter* painter,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const
{
    const Palette& pal = Theme::current();
    const qreal dpr = option.widget ? option.widget->devicePixelRatioF() : 1.0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect cell = option.rect;
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered  = option.state & QStyle::State_MouseOver;

    // --- Card background --------------------------------------------------
    if (selected || hovered) {
        QPainterPath card;
        card.addRoundedRect(QRectF(cell), kCardRadius, kCardRadius);
        painter->fillPath(card, selected ? pal.sel : pal.hover);
    }

    const QRect coverRect(cell.left() + kPad, cell.top() + kPad, kCoverW, kCoverH);

    // --- Cover image (real scan) or gradient placeholder ------------------
    const QString coverPath = index.data(MovieListModel::CoverFrontPathRole).toString();
    const QString title     = index.data(MovieListModel::TitleRole).toString();
    const int     year      = index.data(MovieListModel::YearRole).toInt();
    const QString format    = index.data(MovieListModel::FormatRole).toString();

    QPixmap cover;
    bool real = false;
    if (!coverPath.isEmpty()) {
        const QString key = CoverCache::key(coverPath, QStringLiteral("grid"));
        if (!QPixmapCache::find(key, &cover)) {
            QPixmap raw(coverPath);
            if (!raw.isNull()) {
                const QSize target = QSize(kCoverW, kCoverH) * dpr;
                QPixmap scaled = raw.scaled(target, Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
                const int dx = (scaled.width()  - target.width())  / 2;
                const int dy = (scaled.height() - target.height()) / 2;
                cover = scaled.copy(qMax(0, dx), qMax(0, dy),
                                    target.width(), target.height());
                cover.setDevicePixelRatio(dpr);
                QPixmapCache::insert(key, cover);
            }
        }
        real = !cover.isNull();
    }
    if (!real)
        cover = gridPlaceholder(title, year, format, dpr);

    // Drop shadow under the cover.
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, Theme::isDark() ? 90 : 45));
    painter->drawRoundedRect(QRectF(coverRect).adjusted(2, 5, 2, 6),
                             kCoverRadius, kCoverRadius);
    painter->restore();

    painter->save();
    QPainterPath clip;
    clip.addRoundedRect(QRectF(coverRect), kCoverRadius, kCoverRadius);
    painter->setClipPath(clip);
    painter->drawPixmap(coverRect, cover);
    painter->restore();

    // --- Loan / box-set markers ------------------------------------------
    if (index.data(MovieListModel::IsLoanedRole).toBool()) {
        painter->setPen(QPen(QColor(255, 255, 255, 220), 1.5));
        painter->setBrush(Theme::loanAccent());
        painter->drawEllipse(QRect(coverRect.right() - 16, coverRect.top() + 8, 9, 9));
    }
    if (index.data(MovieListModel::IsBoxSetParentRole).toBool()) {
        QFont bf = option.font;
        bf.setPointSizeF(qMax(7.0, bf.pointSizeF() - 2.0));
        bf.setBold(true);
        painter->setFont(bf);
        const QFontMetrics fm(bf);
        const QRect badge(coverRect.left() + 7, coverRect.bottom() - fm.height() - 9,
                          fm.horizontalAdvance(QStringLiteral("SET")) + 12, fm.height() + 3);
        painter->setPen(Qt::NoPen);
        painter->setBrush(pal.accent);
        painter->drawRoundedRect(badge, 3, 3);
        painter->setPen(pal.accentFg);
        painter->drawText(badge, Qt::AlignCenter, QStringLiteral("SET"));
    }

    // --- Meta (title + year · genre) -------------------------------------
    const int metaLeft  = cell.left() + kPad + 2;
    const int metaRight = cell.right() - kPad - 2;
    const int metaW     = metaRight - metaLeft;

    QFont titleFont = option.font;
    titleFont.setBold(true);
    titleFont.setPointSizeF(qMax(9.0, titleFont.pointSizeF() - 0.5));
    painter->setFont(titleFont);
    const QFontMetrics tfm(titleFont);
    painter->setPen(pal.text);

    // Up to two title lines — greedy word wrap (cheap, no per-char scan).
    QString l1 = title, l2;
    if (tfm.horizontalAdvance(title) > metaW) {
        const QStringList words = title.split(QChar(u' '), Qt::SkipEmptyParts);
        QString cur;
        int i = 0;
        for (; i < words.size(); ++i) {
            const QString trial = cur.isEmpty() ? words[i]
                                                : cur + QChar(u' ') + words[i];
            if (!cur.isEmpty() && tfm.horizontalAdvance(trial) > metaW) break;
            cur = trial;
        }
        l1 = cur.isEmpty() ? title : cur;
        if (tfm.horizontalAdvance(l1) > metaW)
            l1 = tfm.elidedText(l1, Qt::ElideRight, metaW);
        if (i < words.size())
            l2 = tfm.elidedText(QStringList(words.mid(i)).join(QChar(u' ')),
                                Qt::ElideRight, metaW);
    }
    int ty = cell.top() + kMetaTop;
    painter->drawText(QRect(metaLeft, ty, metaW, tfm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, l1);
    if (!l2.isEmpty()) {
        ty += tfm.height();
        painter->drawText(QRect(metaLeft, ty, metaW, tfm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, l2);
    }

    // Sub line.
    QStringList subParts;
    if (year > 0) subParts << QString::number(year);
    const QString genre = firstGenre(index);
    if (!genre.isEmpty()) subParts << genre;
    if (!subParts.isEmpty()) {
        QFont subFont = option.font;
        subFont.setPointSizeF(qMax(8.0, subFont.pointSizeF() - 1.0));
        painter->setFont(subFont);
        const QFontMetrics sfm(subFont);
        painter->setPen(pal.text2);
        const QString sub = sfm.elidedText(subParts.join(QStringLiteral("  ·  ")),
                                           Qt::ElideRight, metaW);
        painter->drawText(QRect(metaLeft, ty + tfm.height() + 1, metaW, sfm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, sub);
    }

    // --- Selection outline ------------------------------------------------
    if (selected) {
        QPen pen(pal.accent, 1.5);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(cell).adjusted(0.75, 0.75, -0.75, -0.75),
                                 kCardRadius, kCardRadius);
    }

    painter->restore();
}

void CoverDelegate::paintBadge(QPainter*, const QRect&, const QColor&, const QString&) const
{
    // Superseded by inline marker painting; retained for ABI of the header.
}

// ---------------------------------------------------------------------------
// CoverGridWidget
// ---------------------------------------------------------------------------
CoverGridWidget::CoverGridWidget(QWidget* parent)
    : QListView(parent)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setGridSize({kTileW, kTileH});
    setUniformItemSizes(true);
    setResizeMode(QListView::Adjust);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMouseTracking(true);   // enable hover state
    setSpacing(10);

    // Smooth pixel scrolling instead of the jumpy item-at-a-time default.
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(24);

    // Keep several screenfuls of rendered tiles cached so scrolling/hover
    // never re-renders a visible cover or placeholder (default 10 MB is too
    // small for a grid of posters).
    if (QPixmapCache::cacheLimit() < 64 * 1024)
        QPixmapCache::setCacheLimit(64 * 1024);

    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFrameShape(QFrame::NoFrame);
    viewport()->setAutoFillBackground(false);

    setItemDelegate(new CoverDelegate(this));
}

void CoverGridWidget::setModel(QAbstractItemModel* model)
{
    QListView::setModel(model);

    // Drive selection from the current index (changes on both mouse click and
    // keyboard arrow navigation) so the detail pane follows cursor movement,
    // not just clicks. selectionModel() only exists after setModel().
    if (auto* sel = selectionModel())
        connect(sel, &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex& cur, const QModelIndex&) {
            onItemActivated(cur);
        });
}

void CoverGridWidget::onItemActivated(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const auto id = index.data(MovieListModel::IdRole).toString();
    if (!id.isEmpty())
        Q_EMIT movieClicked(id);
}

} // namespace xyz
