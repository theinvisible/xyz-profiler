#include "ui/CoverGridWidget.h"

#include "models/MovieListModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace xyz {

// ---------------------------------------------------------------------------
// Layout / colour constants
// ---------------------------------------------------------------------------
namespace {

// Tile geometry
constexpr int kTileW          = 180;
constexpr int kTileH          = 270;
constexpr int kTilePadding    = 6;    // inset from tile edge to cover image
constexpr int kCoverW         = kTileW - 2 * kTilePadding;   // 168
constexpr int kCoverH         = 216;
constexpr int kCoverY         = kTilePadding;
constexpr int kCornerRadius   = 6;
constexpr int kFooterH        = 44;
constexpr int kSelectionBorder = 2;

// Badge geometry
constexpr int kBadgePadH      = 6;
constexpr int kBadgePadV      = 2;
constexpr int kBadgeRadius    = 4;
constexpr int kBadgeMargin    = 4;   // from cover edge

// Colours — matched to vram-task-manager dark palette
const QColor kTileBg          {0x1e, 0x21, 0x28};
const QColor kFooterBg        {0x14, 0x17, 0x1c, 210};
const QColor kTextPrimary     {0xd8, 0xdd, 0xe5};
const QColor kTextSecondary   {0xd8, 0xdd, 0xe5, 204};
const QColor kPlaceholderText {0x8b, 0x91, 0x9e};
const QColor kSelectionColour {0x3a, 0x7b, 0xd5};
const QColor kBadgeLoaned     {0xd3, 0x2f, 0x2f};
const QColor kBadgeBoxSet     {0x3a, 0x7b, 0xd5};

} // anonymous namespace

// ---------------------------------------------------------------------------
// CoverDelegate
// ---------------------------------------------------------------------------
CoverDelegate::CoverDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize CoverDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                              const QModelIndex& /*index*/) const
{
    return {kTileW, kTileH};
}

void CoverDelegate::paint(QPainter* painter,
                           const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRect cell = option.rect;

    // --- 1. Background rounded rect --------------------------------------
    {
        QPainterPath path;
        path.addRoundedRect(QRectF(cell), kCornerRadius, kCornerRadius);
        painter->fillPath(path, kTileBg);
    }

    // Absolute cover rect inside the cell
    const QRect coverRect(cell.left() + kTilePadding,
                          cell.top()  + kCoverY,
                          kCoverW, kCoverH);

    // --- 2. Cover image (or placeholder) ----------------------------------
    const auto coverPath = index.data(MovieListModel::CoverFrontPathRole).toString();
    bool coverPainted = false;

    if (!coverPath.isEmpty()) {
        QPixmap pm;
        if (!QPixmapCache::find(coverPath, &pm)) {
            // Cache miss — load synchronously (local file, fast).
            QPixmap raw(coverPath);
            if (!raw.isNull()) {
                // Scale with KeepAspectRatioByExpanding, then crop centre.
                QPixmap scaled = raw.scaled(
                    coverRect.size() * painter->device()->devicePixelRatioF(),
                    Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
                scaled.setDevicePixelRatio(painter->device()->devicePixelRatioF());

                const int dx = (scaled.width()  / scaled.devicePixelRatio() - coverRect.width())  / 2;
                const int dy = (scaled.height() / scaled.devicePixelRatio() - coverRect.height()) / 2;
                pm = scaled.copy(
                    dx * scaled.devicePixelRatio(),
                    dy * scaled.devicePixelRatio(),
                    coverRect.width()  * scaled.devicePixelRatio(),
                    coverRect.height() * scaled.devicePixelRatio());
                pm.setDevicePixelRatio(scaled.devicePixelRatio());

                QPixmapCache::insert(coverPath, pm);
            }
        }
        if (!pm.isNull()) {
            // Clip to rounded rect so corners stay consistent.
            painter->save();
            QPainterPath clip;
            clip.addRoundedRect(QRectF(coverRect), kCornerRadius - 1, kCornerRadius - 1);
            painter->setClipPath(clip);
            painter->drawPixmap(coverRect, pm);
            painter->restore();
            coverPainted = true;
        }
    }

    if (!coverPainted) {
        // Placeholder: darker fill + format / title text centred.
        painter->save();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(coverRect), kCornerRadius - 1, kCornerRadius - 1);
        painter->setClipPath(clip);
        painter->fillRect(coverRect, QColor(0x25, 0x25, 0x28));

        const auto format = index.data(MovieListModel::FormatRole).toString();
        const auto title  = index.data(MovieListModel::TitleRole).toString();

        QFont fmtFont = painter->font();
        fmtFont.setPointSize(14);
        fmtFont.setBold(true);
        painter->setFont(fmtFont);
        painter->setPen(kPlaceholderText);

        const QRect textArea = coverRect.adjusted(6, 6, -6, -6);
        if (!format.isEmpty()) {
            QRect fmtBound;
            painter->drawText(textArea, Qt::AlignHCenter | Qt::AlignTop, format, &fmtBound);

            // Title below the format label
            QFont titleFont = painter->font();
            titleFont.setPointSize(11);
            titleFont.setBold(false);
            painter->setFont(titleFont);
            const QRect titleArea(textArea.left(),
                                  fmtBound.bottom() + 8,
                                  textArea.width(),
                                  textArea.bottom() - fmtBound.bottom() - 8);
            painter->drawText(titleArea,
                              Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                              title);
        } else {
            painter->drawText(textArea,
                              Qt::AlignCenter | Qt::TextWordWrap,
                              title);
        }
        painter->restore();
    }

    // --- 3. LOANED badge (top-right of cover) ----------------------------
    const bool isLoaned = index.data(MovieListModel::IsLoanedRole).toBool();
    if (isLoaned) {
        const QString label = QStringLiteral("LOANED");
        QFont badgeFont = painter->font();
        badgeFont.setPointSize(10);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);
        const QFontMetrics fm(badgeFont);
        const int textW = fm.horizontalAdvance(label);
        const int textH = fm.height();
        const QRect badge(
            coverRect.right() - textW - 2 * kBadgePadH - kBadgeMargin,
            coverRect.top() + kBadgeMargin,
            textW + 2 * kBadgePadH,
            textH + 2 * kBadgePadV);
        paintBadge(painter, badge, kBadgeLoaned, label);
    }

    // --- 4. SET badge (bottom-left of cover) -----------------------------
    const bool isBoxSetParent = index.data(MovieListModel::IsBoxSetParentRole).toBool();
    if (isBoxSetParent) {
        const QString label = QStringLiteral("SET");
        QFont badgeFont = painter->font();
        badgeFont.setPointSize(10);
        badgeFont.setBold(true);
        painter->setFont(badgeFont);
        const QFontMetrics fm(badgeFont);
        const int textW = fm.horizontalAdvance(label);
        const int textH = fm.height();
        const QRect badge(
            coverRect.left() + kBadgeMargin,
            coverRect.bottom() - textH - 2 * kBadgePadV - kBadgeMargin,
            textW + 2 * kBadgePadH,
            textH + 2 * kBadgePadV);
        paintBadge(painter, badge, kBadgeBoxSet, label);
    }

    // --- 5. Footer bar (title + year/format) -----------------------------
    {
        const QRect footerRect(cell.left(), cell.bottom() - kFooterH,
                               cell.width(), kFooterH);
        // Clip to the bottom rounded corners of the tile
        painter->save();
        QPainterPath tilePath;
        tilePath.addRoundedRect(QRectF(cell), kCornerRadius, kCornerRadius);
        painter->setClipPath(tilePath);
        painter->fillRect(footerRect, kFooterBg);
        painter->restore();

        const int textLeft  = footerRect.left() + 8;
        const int textRight = footerRect.right() - 8;
        const int textW     = textRight - textLeft;

        // Title line
        const auto title = index.data(MovieListModel::TitleRole).toString();
        QFont titleFont = painter->font();
        titleFont.setPointSize(13);
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(kTextPrimary);

        const QFontMetrics titleFm(titleFont);
        const QString elidedTitle = titleFm.elidedText(title, Qt::ElideRight, textW);
        const int titleY = footerRect.top() + 4;
        painter->drawText(QRect(textLeft, titleY, textW, titleFm.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

        // Year + format line
        const int year = index.data(MovieListModel::YearRole).toInt();
        const auto format = index.data(MovieListModel::FormatRole).toString();
        QString sub;
        if (year > 0 && !format.isEmpty())
            sub = QStringLiteral("%1  %2").arg(year).arg(format);
        else if (year > 0)
            sub = QString::number(year);
        else
            sub = format;

        if (!sub.isEmpty()) {
            QFont subFont = painter->font();
            subFont.setPointSize(11);
            subFont.setBold(false);
            painter->setFont(subFont);
            painter->setPen(kTextSecondary);

            const QFontMetrics subFm(subFont);
            const QString elidedSub = subFm.elidedText(sub, Qt::ElideRight, textW);
            const int subY = titleY + titleFm.height() + 1;
            painter->drawText(QRect(textLeft, subY, textW, subFm.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, elidedSub);
        }
    }

    // --- 6. Selection border ---------------------------------------------
    if (option.state & QStyle::State_Selected) {
        QPen pen(kSelectionColour, kSelectionBorder);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        const qreal half = kSelectionBorder / 2.0;
        painter->drawRoundedRect(QRectF(cell).adjusted(half, half, -half, -half),
                                 kCornerRadius, kCornerRadius);
    }

    painter->restore();
}

void CoverDelegate::paintBadge(QPainter* painter, const QRect& rect,
                                const QColor& bg, const QString& text) const
{
    painter->save();
    QPainterPath path;
    path.addRoundedRect(QRectF(rect), kBadgeRadius, kBadgeRadius);
    painter->fillPath(path, bg);
    painter->setPen(kTextPrimary);
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
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
    setSpacing(8);

    // Eliminate the default icon-mode item editor trigger.
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Transparent background — the parent widget provides the overall bg.
    setFrameShape(QFrame::NoFrame);
    viewport()->setAutoFillBackground(false);

    setItemDelegate(new CoverDelegate(this));

    // Wire single-click and keyboard activation.
    connect(this, &QAbstractItemView::clicked,
            this, &CoverGridWidget::onItemActivated);
    connect(this, &QAbstractItemView::activated,
            this, &CoverGridWidget::onItemActivated);
}

void CoverGridWidget::setModel(QAbstractItemModel* model)
{
    QListView::setModel(model);
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
