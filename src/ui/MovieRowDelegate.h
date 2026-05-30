#pragma once

#include <QStyledItemDelegate>

namespace xyz {

// ---------------------------------------------------------------------------
// MovieRowDelegate — themed painting for the collection list (QTreeView).
// ---------------------------------------------------------------------------
// Mirrors the design's list rows: a small cover swatch + title (+ loan dot)
// in the Title column, a coloured format pill in the Format column, and a
// 5-star "my rating" in the Rating column. An accent bar marks the selected
// row. All other columns fall back to the default (QSS-styled) rendering.
// ---------------------------------------------------------------------------
class MovieRowDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit MovieRowDelegate(QObject* parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    void paintTitle(QPainter* p, const QStyleOptionViewItem& opt,
                    const QModelIndex& index) const;
    void paintFormat(QPainter* p, const QStyleOptionViewItem& opt,
                     const QModelIndex& index) const;
    void paintRating(QPainter* p, const QStyleOptionViewItem& opt,
                     const QModelIndex& index) const;
};

} // namespace xyz
