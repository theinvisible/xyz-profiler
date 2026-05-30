#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QStyle>

namespace xyz {

// A simple left-to-right wrapping layout (the canonical Qt "Flow Layout"
// example, trimmed) — used for the detail pane's genre chips so they wrap to
// the next line when the pane is narrow.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = 0,
                        int hSpacing = 6, int vSpacing = 6);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int  horizontalSpacing() const;
    int  verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int  heightForWidth(int width) const override;
    int  count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem*> m_items;
    int m_hSpace;
    int m_vSpace;
};

} // namespace xyz
