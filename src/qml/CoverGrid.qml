import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

GridView {
    id: grid
    clip: true
    cellWidth:  180
    cellHeight: 270

    // ---- Performance settings --------------------------------------------
    // Delegate recycling: when an item scrolls out of view its QQuickItem
    // is rebound to a different model row instead of being destroyed and
    // recreated. Biggest single-knob win for grid-of-N scrolling.
    reuseItems: true
    // Pre-instantiate a few rows above and below the viewport so fast
    // flicking doesn't reveal empty cells while delegates spin up.
    cacheBuffer: cellHeight * 4
    // Don't decelerate flicks artificially — feels more responsive on
    // long lists.
    flickDeceleration: 4500
    maximumFlickVelocity: 6000

    // Signal carrying the movie id of the tile the user clicked.
    signal movieClicked(string id)

    // ---- Pre-computed style values ---------------------------------------
    // Each delegate would otherwise re-call `Material.color(...)` and
    // `Qt.rgba(...)` on construction. Lifting them here means they're
    // evaluated once for the whole grid and shared by reference.
    readonly property color tileBg:      Material.color(Material.Grey, Material.Shade800)
    readonly property color footerBg:    Qt.rgba(0, 0, 0, 0.55)
    readonly property color loanedBg:    Material.color(Material.Red,  Material.Shade700)
    readonly property color setBg:       Material.color(Material.Blue, Material.Shade700)
    readonly property color borderColor: Material.accent

    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    delegate: Item {
        id: cell
        width:  grid.cellWidth
        height: grid.cellHeight

        // Flat, anchor-based composition — no layouts in the hot path.
        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            color:  grid.tileBg
            radius: 6
            border.width: GridView.isCurrentItem ? 2 : 0
            border.color: grid.borderColor

            // ---- Cover image (or hidden when no source) -----------------
            Image {
                id: cover
                anchors.left:   parent.left
                anchors.right:  parent.right
                anchors.top:    parent.top
                anchors.bottom: footer.top
                source: model.coverFrontPath || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                // Decode at the actual display size, not the full
                // source resolution — DVD covers are often 600×900+.
                sourceSize.width:  grid.cellWidth
                sourceSize.height: grid.cellHeight - 44
                visible: source !== "" && status === Image.Ready
            }

            // ---- Placeholder (only when there's no cover) ---------------
            Item {
                anchors.fill: cover
                visible: !cover.visible

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter:   parent.verticalCenter
                    anchors.verticalCenterOffset: -28
                    text: model.format || "—"
                    font.pixelSize: 14
                    color: "white"
                    opacity: 0.55
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter:   parent.verticalCenter
                    anchors.verticalCenterOffset: 4
                    width: parent.width - 16
                    text: model.title
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 14
                    color: "white"
                    opacity: 0.85
                }
            }

            // ---- Loaned badge (top-right) -------------------------------
            Rectangle {
                visible: model.isLoaned
                anchors.top:    parent.top
                anchors.right:  parent.right
                anchors.margins: 6
                width: 64
                height: 18
                radius: 4
                color: grid.loanedBg
                Text {
                    anchors.centerIn: parent
                    text: "LOANED"
                    font.pixelSize: 10
                    font.bold: true
                    color: "white"
                }
            }

            // ---- Box-set badge (bottom-left of cover area) --------------
            Rectangle {
                visible: model.isBoxSetParent
                anchors.left:   parent.left
                anchors.bottom: footer.top
                anchors.margins: 6
                width: 36
                height: 18
                radius: 4
                color: grid.setBg
                Text {
                    anchors.centerIn: parent
                    text: "SET"
                    font.pixelSize: 10
                    font.bold: true
                    color: "white"
                }
            }

            // ---- Footer with title + year/format ------------------------
            Rectangle {
                id: footer
                anchors.left:   parent.left
                anchors.right:  parent.right
                anchors.bottom: parent.bottom
                height: 44
                color: grid.footerBg

                Text {
                    anchors.left:  parent.left
                    anchors.right: parent.right
                    anchors.top:   parent.top
                    anchors.leftMargin:  8
                    anchors.rightMargin: 8
                    anchors.topMargin:   4
                    text: model.title
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                    color: "white"
                }
                Text {
                    anchors.left:   parent.left
                    anchors.right:  parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin:   8
                    anchors.rightMargin:  8
                    anchors.bottomMargin: 6
                    text: (model.year ? model.year : "") +
                          (model.format ? "  ·  " + model.format : "")
                    font.pixelSize: 11
                    opacity: 0.8
                    elide: Text.ElideRight
                    color: "white"
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    grid.currentIndex = index;
                    grid.movieClicked(model.id);
                }
            }
        }
    }
}
