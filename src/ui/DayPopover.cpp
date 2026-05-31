#include "ui/DayPopover.h"

#include "ui/Theme.h"

#include <QGuiApplication>
#include <QLabel>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

namespace xyz {

DayPopover::DayPopover(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setObjectName(QStringLiteral("dayPopover"));
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 8, 10, 10);
    lay->setSpacing(6);

    m_header = new QLabel(this);
    QFont hf = m_header->font();
    hf.setBold(true);
    m_header->setFont(hf);
    lay->addWidget(m_header);

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(28, 42));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(true);
    m_list->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* it) {
        const QString id = it->data(Qt::UserRole).toString();
        hide();
        if (!id.isEmpty()) emit movieActivated(id);
    });

    refreshTheme();
}

void DayPopover::refreshTheme()
{
    const Palette& pal = Theme::current();
    setStyleSheet(QStringLiteral(
        "#dayPopover { background:%1; border:1px solid %2; }"
        "QListWidget { background:transparent; border:none; }"
        "QListWidget::item { padding:5px 6px; border-radius:6px; color:%3; }"
        "QListWidget::item:hover { background:%4; }"
        "QListWidget::item:selected { background:%5; color:%6; }")
        .arg(pal.panel.name(), pal.borderStrong.name(), pal.text.name(),
             pal.hover.name(), pal.accent.name(), pal.accentFg.name()));
    m_header->setStyleSheet(QStringLiteral("color:%1;").arg(pal.text2.name()));
}

void DayPopover::showFor(const QString& header, const QList<Entry>& entries,
                         const QPoint& anchorGlobal)
{
    m_header->setText(header);
    m_list->clear();
    for (const Entry& e : entries) {
        const QString label = e.year > 0
            ? QStringLiteral("%1  ·  %2").arg(e.title, QString::number(e.year))
            : e.title;
        auto* it = new QListWidgetItem(e.icon, label, m_list);
        it->setData(Qt::UserRole, e.id);
    }

    QScreen* scr = QGuiApplication::screenAt(anchorGlobal);
    if (!scr) scr = QGuiApplication::primaryScreen();
    const QRect avail = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);

    const int rows = qBound(1, int(entries.size()), 9);
    const QSize sz(330, 44 + rows * 52);
    setFixedSize(sz);

    QPoint pos = anchorGlobal + QPoint(10, 10);
    pos.setX(qBound(avail.left() + 8, pos.x(), avail.right() - sz.width() - 8));
    pos.setY(qBound(avail.top() + 8, pos.y(), avail.bottom() - sz.height() - 8));
    move(pos);
    show();
    raise();
}

} // namespace xyz
