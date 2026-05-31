#pragma once

#include <QWidget>

namespace xyz {

struct Movie;
class MovieDetailWidget;

// A floating, frameless Qt::Popup that hosts the reused MovieDetailWidget. Shown
// anchored next to a clicked film in the Calendar; closes automatically on
// click-outside / Esc (inherent to Qt::Popup). Re-emits the detail widget's
// TMDb-search request so the caller can route it to the controller.
class MoviePopover : public QWidget {
    Q_OBJECT

public:
    explicit MoviePopover(QWidget* parent = nullptr);

    void showFor(const Movie& movie, const QPoint& anchorGlobal);
    void refreshTheme();

signals:
    void tmdbSearchRequested();

private:
    MovieDetailWidget* m_detail = nullptr;
};

} // namespace xyz
