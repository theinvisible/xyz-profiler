import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

import xyz.profiler

// Modal settings editor. Form fields are seeded from SettingsController
// on open; changes are applied to the controller on "Save" so the user
// can cancel without persisting half-edits. Setters on the controller
// already write to disk synchronously, so closing the dialog is enough.
Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    title: qsTr("Settings")
    standardButtons: Dialog.Save | Dialog.Cancel
    implicitWidth: Math.min(parent ? parent.width - 80 : 560, 560)

    // Local working copy — populated each time the dialog opens.
    property string editApiKey:    ""
    property string editImagesDir: ""
    property string editTheme:     "Dark"

    onAboutToShow: {
        editApiKey    = SettingsController.tmdbApiKey;
        editImagesDir = SettingsController.imagesDirectory;
        editTheme     = SettingsController.themeName;
    }

    onAccepted: {
        SettingsController.tmdbApiKey      = editApiKey;
        SettingsController.imagesDirectory = editImagesDir;
        SettingsController.themeName       = editTheme;
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // ---- TMDb API key -----------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label {
                text: qsTr("TMDb API key")
                font.bold: true
            }
            Label {
                text: qsTr("Free key from themoviedb.org. Overrides the TMDB_API_KEY environment variable.")
                opacity: 0.65
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: apiKeyField
                    Layout.fillWidth: true
                    text: root.editApiKey
                    placeholderText: qsTr("0123456789abcdef…")
                    echoMode: showKey.checked ? TextInput.Normal : TextInput.Password
                    onTextChanged: root.editApiKey = text
                }
                CheckBox {
                    id: showKey
                    text: qsTr("Show")
                }
            }

            // TMDb attribution — required by the TMDB API terms of use
            // (section 3). The logo identifies the data source; the notice
            // disclaims endorsement. Both must appear wherever the API is
            // used, so the natural home is next to the API-key field.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 10

                Image {
                    source: "qrc:/tmdb_logo.svg"
                    sourceSize.height: 22
                    fillMode: Image.PreserveAspectFit
                    Layout.preferredHeight: 22
                    Layout.alignment: Qt.AlignVCenter

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://www.themoviedb.org/")
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("This product uses TMDB and the TMDB APIs but is not endorsed, certified, or otherwise approved by TMDB.")
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    font.pixelSize: 11
                }
            }
        }

        // ---- Images directory -------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label {
                text: qsTr("Cover images directory")
                font.bold: true
            }
            Label {
                text: qsTr("Default location of DVD Profiler-exported cover JPEGs. Used during import.")
                opacity: 0.65
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: imagesField
                    Layout.fillWidth: true
                    text: root.editImagesDir
                    placeholderText: qsTr("(none)")
                    onTextChanged: root.editImagesDir = text
                }
                Button {
                    text: qsTr("Browse…")
                    onClicked: imagesFolderDialog.open()
                }
            }
        }

        // ---- Theme ------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label {
                text: qsTr("Theme")
                font.bold: true
            }
            ComboBox {
                id: themeCombo
                Layout.fillWidth: true
                model: ["Dark", "Light", "System"]
                currentIndex: model.indexOf(root.editTheme) >= 0
                    ? model.indexOf(root.editTheme) : 0
                onActivated: root.editTheme = model[currentIndex]
            }
        }

        Item { Layout.preferredHeight: 4 }
    }

    FolderDialog {
        id: imagesFolderDialog
        title: qsTr("Pick the cover images directory")
        onAccepted: root.editImagesDir =
            SettingsController.urlToLocalPath(selectedFolder.toString())
    }
}
