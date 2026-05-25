#pragma once

#include <QListView>
#include <QStyledItemDelegate>

class QAbstractItemModel;

namespace xyz {

// ---------------------------------------------------------------------------
// CoverDelegate — custom-painted DVD/Blu-ray cover tile for the grid.
// ---------------------------------------------------------------------------
// Each tile is 180x270 with:
//   - Dark rounded-rect background
//   - Cover image (scaled-to-fill, QPixmapCache'd) or placeholder
//   - LOANED badge (top-right, red) and SET badge (bottom-left, blue)
//   - Semi-transparent footer bar with elided title + year/format
//   - Blue selection border
//
// This is a private implementation detail of CoverGridWidget; external code
// should not depend on it.
// ---------------------------------------------------------------------------
class CoverDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit CoverDelegate(QObject* parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    void paintBadge(QPainter* painter, const QRect& rect,
                    const QColor& bg, const QString& text) const;
};

// ---------------------------------------------------------------------------
// CoverGridWidget — QListView in IconMode showing cover tiles.
// ---------------------------------------------------------------------------
class CoverGridWidget : public QListView {
    Q_OBJECT

public:
    explicit CoverGridWidget(QWidget* parent = nullptr);

    // Convenience: set the model and wire clicked/activated to movieClicked.
    void setModel(QAbstractItemModel* model) override;

Q_SIGNALS:
    /// Emitted when the user clicks or activates a tile.
    /// @param id  Movie primary ID (from IdRole).
    void movieClicked(const QString& id);

private:
    void onItemActivated(const QModelIndex& index);
};

} // namespace xyz
