import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import xyz.profiler

// Confirmation step between the file picker and the (slow) DB-write.
// Auto-shows when the parse worker has produced a preview; closes when
// the user commits or cancels. The controller emits importStateChanged
// for every relevant property, so this dialog updates reactively.
Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    title: qsTr("Confirm import")
    standardButtons: Dialog.NoButton
    closePolicy: Popup.NoAutoClose
    visible: LibraryController.previewActive
    implicitWidth: Math.min(parent ? parent.width - 80 : 560, 560)

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: qsTr("Ready to import %1 movies from %2.")
                .arg(LibraryController.previewMovieCount)
                .arg(LibraryController.previewSourceName)
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            visible: LibraryController.previewImagesDir !== ""
            text: qsTr("Images directory: %1").arg(LibraryController.previewImagesDir)
            opacity: 0.65
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }

        // ---- Sample titles -------------------------------------------------
        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            ColumnLayout {
                anchors.fill: parent
                spacing: 4
                Label {
                    text: qsTr("First %1 of %2:")
                        .arg(LibraryController.previewSampleTitles.length)
                        .arg(LibraryController.previewMovieCount)
                    font.bold: true
                    font.pixelSize: 11
                    opacity: 0.7
                }
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: titleColumn.implicitHeight
                    clip: true
                    ColumnLayout {
                        id: titleColumn
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: LibraryController.previewSampleTitles
                            Label {
                                required property var modelData
                                Layout.fillWidth: true
                                text: "• " + modelData
                                elide: Text.ElideRight
                                opacity: 0.9
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Existing rows with matching IDs will be replaced.")
            opacity: 0.6
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        // ---- Action buttons -----------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Cancel")
                onClicked: LibraryController.cancelImport()
            }
            Button {
                text: qsTr("Import")
                highlighted: true
                onClicked: LibraryController.commitImport()
            }
        }
    }
}
