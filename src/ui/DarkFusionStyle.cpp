#include "ui/DarkFusionStyle.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

namespace xyz {

// ---------------------------------------------------------------------------
// Dark-mode stylesheet — fine-tunes widgets that QPalette alone can't reach.
// Applied only when themeName == "Dark".
// ---------------------------------------------------------------------------
static const char* darkStyleSheet()
{
    return R"(
        QToolTip {
            background-color: #3c3c3f;
            color:            #d4d4d4;
            border:           1px solid #555555;
            padding:          4px;
        }

        QHeaderView::section {
            background-color: #333337;
            color:            #d4d4d4;
            padding:          4px 6px;
            border:           none;
            border-right:     1px solid #555555;
            border-bottom:    1px solid #555555;
        }

        QScrollBar:vertical {
            background: #1e1e1e;
            width:      14px;
            margin:     0;
        }
        QScrollBar::handle:vertical {
            background:    #555555;
            min-height:    24px;
            border-radius: 4px;
            margin:        2px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6a6a6a;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }

        QScrollBar:horizontal {
            background: #1e1e1e;
            height:     14px;
            margin:     0;
        }
        QScrollBar::handle:horizontal {
            background:    #555555;
            min-width:     24px;
            border-radius: 4px;
            margin:        2px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #6a6a6a;
        }
        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0;
        }

        QToolBar {
            background: #2d2d30;
            border:     none;
            spacing:    4px;
            padding:    2px;
        }

        QMenuBar {
            background-color: #2d2d30;
            color:            #d4d4d4;
            border-bottom:    1px solid #3f3f46;
        }
        QMenuBar::item:selected {
            background-color: #3e3e42;
        }
        QMenuBar::item:pressed {
            background-color: #2979ff;
            color:            #ffffff;
        }

        QMenu {
            background-color: #2d2d30;
            color:            #d4d4d4;
            border:           1px solid #3f3f46;
        }
        QMenu::item:selected {
            background-color: #3e3e42;
        }

        QStatusBar {
            background: #1e1e1e;
            color:      #a0a0a0;
            border-top: 1px solid #3f3f46;
        }
    )";
}

// ---------------------------------------------------------------------------
// Dark palette — Material-Blue accent on a VS-Code-ish dark background.
// ---------------------------------------------------------------------------
static QPalette darkPalette()
{
    QPalette pal;

    // Active / Inactive roles
    pal.setColor(QPalette::Window,          QColor(0x2d, 0x2d, 0x30));
    pal.setColor(QPalette::WindowText,      QColor(0xd4, 0xd4, 0xd4));
    pal.setColor(QPalette::Base,            QColor(0x1e, 0x1e, 0x1e));
    pal.setColor(QPalette::AlternateBase,   QColor(0x2d, 0x2d, 0x30));
    pal.setColor(QPalette::Text,            QColor(0xd4, 0xd4, 0xd4));
    pal.setColor(QPalette::Button,          QColor(0x3c, 0x3c, 0x3f));
    pal.setColor(QPalette::ButtonText,      QColor(0xd4, 0xd4, 0xd4));
    pal.setColor(QPalette::Highlight,       QColor(0x29, 0x79, 0xff));  // Material Blue
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::BrightText,      QColor(0xff, 0x52, 0x52));  // red for warnings
    pal.setColor(QPalette::Link,            QColor(0x42, 0xa5, 0xf5));
    pal.setColor(QPalette::LinkVisited,     QColor(0xce, 0x93, 0xd8));
    pal.setColor(QPalette::ToolTipBase,     QColor(0x3c, 0x3c, 0x3f));
    pal.setColor(QPalette::ToolTipText,     QColor(0xd4, 0xd4, 0xd4));
    pal.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));

    // Disabled group — everything dimmed so inactive controls recede.
    pal.setColor(QPalette::Disabled, QPalette::WindowText,      QColor(0x70, 0x70, 0x70));
    pal.setColor(QPalette::Disabled, QPalette::Text,            QColor(0x70, 0x70, 0x70));
    pal.setColor(QPalette::Disabled, QPalette::ButtonText,      QColor(0x70, 0x70, 0x70));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x70, 0x70, 0x70));
    pal.setColor(QPalette::Disabled, QPalette::Highlight,       QColor(0x3c, 0x3c, 0x3f));
    pal.setColor(QPalette::Disabled, QPalette::Base,            QColor(0x24, 0x24, 0x27));
    pal.setColor(QPalette::Disabled, QPalette::Button,          QColor(0x33, 0x33, 0x36));

    return pal;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void DarkFusionStyle::applyTheme(const QString& themeName)
{
    if (themeName == QStringLiteral("Dark")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(darkPalette());
        qApp->setStyleSheet(QString::fromLatin1(darkStyleSheet()));
    } else if (themeName == QStringLiteral("Light")) {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        QApplication::setPalette(QPalette());   // default light palette
        qApp->setStyleSheet(QString());
    } else {
        // "System" — platform-native style, default palette, no overrides.
        QApplication::setStyle(QStyleFactory::create(QString()));
        QApplication::setPalette(QPalette());
        qApp->setStyleSheet(QString());
    }
}

} // namespace xyz
