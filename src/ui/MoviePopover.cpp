#include "ui/MoviePopover.h"

#include "ui/MovieDetailWidget.h"
#include "ui/Theme.h"

#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>

namespace xyz {

MoviePopover::MoviePopover(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("moviePopover"));
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 1, 1, 1);
    m_detail = new MovieDetailWidget(this);
    m_detail->setFixedWidth(470);
    lay->addWidget(m_detail);
    connect(m_detail, &MovieDetailWidget::tmdbSearchRequested,
            this, &MoviePopover::tmdbSearchRequested);
    refreshTheme();
}

void MoviePopover::refreshTheme()
{
    const Palette& pal = Theme::current();
    setStyleSheet(QStringLiteral("#moviePopover { background:%1; border:1px solid %2; }")
                      .arg(pal.panel.name(), pal.borderStrong.name()));
    m_detail->refreshTheme();
}

void MoviePopover::showFor(const Movie& movie, const QPoint& anchorGlobal)
{
    m_detail->updateFromMovie(movie);

    QScreen* scr = QGuiApplication::screenAt(anchorGlobal);
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);

    const QSize sz(472, qMin(660, avail.height() - 40));
    setFixedSize(sz);

    QPoint pos = anchorGlobal + QPoint(14, -40);
    if (pos.x() + sz.width() > avail.right())
        pos.setX(anchorGlobal.x() - sz.width() - 14);
    pos.setX(qBound(avail.left() + 8, pos.x(), avail.right() - sz.width() - 8));
    pos.setY(qBound(avail.top() + 8, pos.y(), avail.bottom() - sz.height() - 8));

    move(pos);
    show();
    raise();
}

} // namespace xyz
