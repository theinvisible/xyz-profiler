#include "ui/DarkFusionStyle.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

namespace xyz {

static QPalette darkPalette()
{
    QPalette p;
    const QColor bg       (0x14, 0x17, 0x1c);
    const QColor surface  (0x1e, 0x21, 0x28);
    const QColor surfaceAlt(0x25, 0x28, 0x30);
    const QColor text     (0xd8, 0xdd, 0xe5);
    const QColor mutedText(0x8b, 0x91, 0x9e);
    const QColor accent   (0x3a, 0x7b, 0xd5);

    p.setColor(QPalette::Window,          bg);
    p.setColor(QPalette::WindowText,      text);
    p.setColor(QPalette::Base,            surface);
    p.setColor(QPalette::AlternateBase,   surfaceAlt);
    p.setColor(QPalette::ToolTipBase,     surfaceAlt);
    p.setColor(QPalette::ToolTipText,     text);
    p.setColor(QPalette::Text,            text);
    p.setColor(QPalette::Button,          surface);
    p.setColor(QPalette::ButtonText,      text);
    p.setColor(QPalette::BrightText,      Qt::white);
    p.setColor(QPalette::Highlight,       accent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, mutedText);
    p.setColor(QPalette::Link,            accent);

    p.setColor(QPalette::Disabled, QPalette::Text,       mutedText);
    p.setColor(QPalette::Disabled, QPalette::WindowText,  mutedText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText,  mutedText);
    p.setColor(QPalette::Disabled, QPalette::Highlight,   surfaceAlt);

    return p;
}

QString DarkFusionStyle::darkStyleSheet()
{
    return QStringLiteral(R"(
QWidget#central {
    background-color: #14171c;
}
QToolTip {
    background-color: #252830;
    color: #d8dde5;
    border: 1px solid #2a2e38;
    padding: 4px;
}
QToolBar {
    background: #14171c;
    border: none;
    spacing: 4px;
    padding: 2px 4px;
}
QToolBar QToolButton {
    background-color: #1e2128;
    border: 1px solid #2a2e38;
    border-radius: 4px;
    color: #d8dde5;
    padding: 4px 10px;
}
QToolBar QToolButton:hover {
    border-color: #3a7bd5;
    background-color: #232732;
}
QToolBar QToolButton:checked {
    background-color: #3a7bd5;
    color: #ffffff;
}
QToolBar QLineEdit {
    background-color: #1e2128;
    border: 1px solid #2a2e38;
    border-radius: 8px;
    padding: 7px 12px;
    color: #d8dde5;
    selection-background-color: #3a7bd5;
}
QToolBar QLineEdit:focus {
    border-color: #3a7bd5;
}
QTableView, QTreeView {
    background-color: #1a1d24;
    alternate-background-color: #1e2128;
    border: 1px solid #2a2e38;
    border-radius: 10px;
    gridline-color: transparent;
    color: #d8dde5;
    selection-background-color: #3a7bd5;
    selection-color: #ffffff;
    outline: none;
}
QTableView::item, QTreeView::item {
    padding: 4px 6px;
    border: none;
}
QTableView::item:selected, QTreeView::item:selected {
    background-color: #3a7bd5;
    color: #ffffff;
}
QTreeView::item:hover {
    background-color: #252830;
}
QTreeView::item:selected:hover {
    background-color: #3a7bd5;
}
QTreeView::branch {
    background: transparent;
}
QTreeView::branch:has-children:!has-siblings:closed,
QTreeView::branch:closed:has-children:has-siblings {
    border-image: none;
    image: url(:/branch-closed.svg);
}
QTreeView::branch:open:has-children:!has-siblings,
QTreeView::branch:open:has-children:has-siblings {
    border-image: none;
    image: url(:/branch-open.svg);
}
QHeaderView::section {
    background-color: #1e2128;
    color: #8b919e;
    padding: 9px 8px;
    border: none;
    border-bottom: 1px solid #2a2e38;
    font-weight: 600;
    font-size: 10px;
    letter-spacing: 0.7px;
}
QHeaderView::section:hover {
    color: #d8dde5;
}
QTableCornerButton::section {
    background-color: #1e2128;
    border: none;
    border-bottom: 1px solid #2a2e38;
}
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 4px 2px 4px 0;
}
QScrollBar::handle:vertical {
    background: #2f333d;
    border-radius: 4px;
    min-height: 30px;
}
QScrollBar::handle:vertical:hover {
    background: #3f4451;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0 4px 2px 4px;
}
QScrollBar::handle:horizontal {
    background: #2f333d;
    border-radius: 4px;
    min-width: 30px;
}
QScrollBar::handle:horizontal:hover {
    background: #3f4451;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}
QMenuBar {
    background-color: #14171c;
    color: #d8dde5;
    border-bottom: 1px solid #2a2e38;
}
QMenuBar::item:selected {
    background-color: #252830;
}
QMenu {
    background-color: #1e2128;
    color: #d8dde5;
    border: 1px solid #2a2e38;
}
QMenu::item:selected {
    background-color: #3a7bd5;
}
QStatusBar {
    background: #14171c;
    color: #8b919e;
    border-top: 1px solid #2a2e38;
    font-size: 11px;
}
QSplitter::handle {
    background: #2a2e38;
}
QScrollArea {
    border: none;
    background: transparent;
}
QLabel#footer {
    color: #8b919e;
    font-size: 11px;
}
)");
}

void DarkFusionStyle::applyTheme(const QString& themeName)
{
    if (themeName == QStringLiteral("Dark")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(darkPalette());

        QFont font(QStringLiteral("Segoe UI Variable Display"));
        if (!QFontDatabase::families().contains(font.family()))
            font.setFamily(QStringLiteral("Segoe UI"));
        font.setPointSize(10);
        QApplication::setFont(font);
    } else if (themeName == QStringLiteral("Light")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(QPalette());
        QApplication::setFont(QFont());
    } else {
        QApplication::setStyle(QStyleFactory::create(QString()));
        QApplication::setPalette(QPalette());
        QApplication::setFont(QFont());
    }
}

} // namespace xyz
