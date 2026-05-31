#pragma once

#include <QIcon>
#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;

namespace xyz {

// A small frameless Qt::Popup listing all films on a given day (used when a day
// cell holds more films than its mini-covers can show). Each row is clickable
// and re-emits the film id so the caller can open the detail popover.
class DayPopover : public QWidget {
    Q_OBJECT

public:
    struct Entry {
        QString id;
        QString title;
        int     year = 0;
        QIcon   icon;
    };

    explicit DayPopover(QWidget* parent = nullptr);

    void showFor(const QString& header, const QList<Entry>& entries,
                 const QPoint& anchorGlobal);
    void refreshTheme();

signals:
    void movieActivated(const QString& id);

private:
    QLabel*      m_header = nullptr;
    QListWidget* m_list   = nullptr;
};

} // namespace xyz
