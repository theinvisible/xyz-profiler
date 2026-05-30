#include "ui/MovieRowDelegate.h"

#include "models/MovieTreeModel.h"
#include "ui/CoverArt.h"
#include "ui/CoverCache.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace xyz {
namespace {

constexpr int kRowH      = 38;   // --row-h (regular density)
constexpr int kCoverW    = 22;
constexpr int kCoverH    = 32;
constexpr int kStarPx    = 13;
constexpr int kStarGap   = 1;

QPixmap starPixmap(bool on, qreal dpr)
{
    const QColor c = on ? Theme::starOn() : Theme::starOff();
    const QString key = QStringLiteral("xprow_star_%1_%2_%3")
        .arg(c.name(), QString::number(kStarPx), QString::number(dpr));
    QPixmap pm;
    if (!QPixmapCache::find(key, &pm)) {
        pm = IconFactory::pixmap(QStringLiteral("star"), c, kStarPx, 1.0, dpr);
        QPixmapCache::insert(key, pm);
    }
    return pm;
}

QPixmap rowCover(const QString& path, const QString& title,
                 const QString& format, qreal dpr)
{
    const QString id = path.isEmpty() ? title : path;
    const QString key = CoverCache::key(id, QStringLiteral("row_%1").arg(dpr));
    QPixmap pm;
    if (QPixmapCache::find(key, &pm)) return pm;

    if (!path.isEmpty()) {
        QPixmap raw(path);
        if (!raw.isNull()) {
            const QSize target = QSize(kCoverW, kCoverH) * dpr;
            QPixmap scaled = raw.scaled(target, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
            const int dx = (scaled.width()  - target.width())  / 2;
            const int dy = (scaled.height() - target.height()) / 2;
            pm = scaled.copy(qMax(0, dx), qMax(0, dy),
                             target.width(), target.height());
            pm.setDevicePixelRatio(dpr);
        }
    }
    if (pm.isNull())
        pm = CoverArt::placeholder(title, 0, format, QSize(kCoverW, kCoverH), false, dpr);

    QPixmapCache::insert(key, pm);
    return pm;
}

} // namespace

MovieRowDelegate::MovieRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{}

QSize MovieRowDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(qMax(kRowH, s.height()));
    return s;
}

void MovieRowDelegate::paint(QPainter* painter,
                             const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const int col = index.column();
    if (col != MovieTreeModel::Title &&
        col != MovieTreeModel::Format &&
        col != MovieTreeModel::Rating) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const Palette& p = Theme::current();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Row background (match QSS sel/hover so custom + default cells agree).
    if (option.state & QStyle::State_Selected)
        painter->fillRect(option.rect, p.sel);
    else if (option.state & QStyle::State_MouseOver)
        painter->fillRect(option.rect, p.hover);

    if (col == MovieTreeModel::Title)  paintTitle(painter, option, index);
    else if (col == MovieTreeModel::Format) paintFormat(painter, option, index);
    else if (col == MovieTreeModel::Rating) paintRating(painter, option, index);

    painter->restore();
}

void MovieRowDelegate::paintTitle(QPainter* p, const QStyleOptionViewItem& opt,
                                  const QModelIndex& index) const
{
    const Palette& pal = Theme::current();
    const QRect r = opt.rect;
    const qreal dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;

    // Accent selection bar.
    if (opt.state & QStyle::State_Selected)
        p->fillRect(QRect(r.left(), r.top(), 3, r.height()), pal.accent);

    int x = r.left() + 8;

    // Cover swatch.
    const QRect coverRect(x, r.top() + (r.height() - kCoverH) / 2, kCoverW, kCoverH);
    {
        const QString path   = index.data(MovieTreeModel::CoverPathRole).toString();
        const QString title  = index.data(Qt::DisplayRole).toString();
        const QString format = index.data(MovieTreeModel::FormatNameRole).toString();
        const QPixmap pm = rowCover(path, title, format, dpr);
        p->save();
        QPainterPath clip;
        clip.addRoundedRect(QRectF(coverRect), 3, 3);
        p->setClipPath(clip);
        p->drawPixmap(coverRect, pm);
        p->restore();
        p->setPen(QPen(QColor(0, 0, 0, 40), 1));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(coverRect).adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
    }
    x = coverRect.right() + 11;

    // Title text (+ loan dot).
    const bool loaned = index.data(MovieTreeModel::IsLoanedRole).toBool();
    const int dotSpace = loaned ? 14 : 0;

    QFont f = opt.font;
    const QVariant fontVar = index.data(Qt::FontRole);
    if (fontVar.isValid() && fontVar.value<QFont>().bold()) f.setBold(true);
    else f.setWeight(QFont::Medium);
    p->setFont(f);
    const QFontMetrics fm(f);

    const int textW = r.right() - 8 - x - dotSpace;
    const QString title = index.data(Qt::DisplayRole).toString();
    const QString elided = fm.elidedText(title, Qt::ElideRight, qMax(0, textW));
    p->setPen(pal.text);
    p->drawText(QRect(x, r.top(), qMax(0, textW), r.height()),
                Qt::AlignLeft | Qt::AlignVCenter, elided);

    if (loaned) {
        const int dotX = x + fm.horizontalAdvance(elided) + 6;
        const int dotY = r.center().y() - 3;
        p->setPen(Qt::NoPen);
        p->setBrush(Theme::loanAccent());
        p->drawEllipse(QRect(dotX, dotY, 7, 7));
    }
}

void MovieRowDelegate::paintFormat(QPainter* p, const QStyleOptionViewItem& opt,
                                   const QModelIndex& index) const
{
    const QString raw = index.data(Qt::DisplayRole).toString();
    if (raw.isEmpty()) return;
    const auto fb = Theme::formatBadge(raw);

    QFont f = opt.font;
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
    f.setBold(true);
    p->setFont(f);
    const QFontMetrics fm(f);

    const int h = fm.height() + 4;
    const int w = fm.horizontalAdvance(fb.label) + 14;
    const QRect r = opt.rect;
    const QRect badge(r.left() + 8, r.top() + (r.height() - h) / 2, w, h);

    QColor bg = fb.tinted ? fb.fg : Theme::current().panel3;
    bg.setAlpha(fb.tinted ? 28 : 0);
    QColor bd = fb.tinted ? fb.fg : Theme::current().borderStrong;
    bd.setAlpha(fb.tinted ? 110 : 255);

    p->setBrush(bg);
    p->setPen(QPen(bd, 1));
    p->drawRoundedRect(QRectF(badge).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    p->setPen(fb.fg);
    p->drawText(badge, Qt::AlignCenter, fb.label);
}

void MovieRowDelegate::paintRating(QPainter* p, const QStyleOptionViewItem& opt,
                                   const QModelIndex& index) const
{
    const int film = index.data(MovieTreeModel::ReviewFilmRole).toInt(); // 0-10
    if (film <= 0) return;
    const int filled = qBound(0, qRound(film / 2.0), 5);
    const qreal dpr = opt.widget ? opt.widget->devicePixelRatioF() : 1.0;

    const int totalW = 5 * kStarPx + 4 * kStarGap;
    const QRect r = opt.rect;
    int sx = r.left() + qMax(8, (r.width() - totalW) / 2);
    const int sy = r.center().y() - kStarPx / 2;

    for (int i = 0; i < 5; ++i) {
        p->drawPixmap(sx, sy, starPixmap(i < filled, dpr));
        sx += kStarPx + kStarGap;
    }
}

} // namespace xyz
