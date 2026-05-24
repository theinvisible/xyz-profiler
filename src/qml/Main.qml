import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

import xyz.profiler

ApplicationWindow {
    id: window
    visible: true
    width: 1400
    height: 900
    title: qsTr("xyz-profiler")

    // Theme tracks the SettingsController; switches live without restart.
    Material.theme: SettingsController.themeName === "Light"  ? Material.Light
                  : SettingsController.themeName === "System" ? Material.System
                  :                                              Material.Dark
    Material.accent: Material.Blue


    // ---- Top bar: search + import ----------------------------------------
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 12

            ToolButton {
                text: qsTr("Import Collection.xml…")
                onClicked: importXmlDialog.open()
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: qsTr("Search title, actor, director, studio, overview…")
                onAccepted: LibraryController.search(text)
                onTextChanged: if (text === "") LibraryController.refresh()
                enabled: LibraryController.movieCount > 0
            }

            Label {
                text: qsTr("%1 movies").arg(LibraryController.movieCount)
                color: Material.foreground
                opacity: 0.7
            }

            ToolButton {
                text: qsTr("Settings…")
                onClicked: settingsDialog.open()
            }
        }
    }

    footer: ToolBar {
        height: 28
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                Layout.fillWidth: true
                text: LibraryController.statusMessage
                elide: Text.ElideRight
                opacity: 0.7
                font.pixelSize: 12
            }
        }
    }

    // ---- Main split: cover grid (left) + detail pane (right) -------------
    // Use a single Item container with explicit x positioning — anchors
    // with forward id references silently collapse the detail pane to
    // zero width on this Qt 6.11 + Material build.
    Item {
        id: bodyContainer
        anchors.fill: parent

        CoverGrid {
            id: grid
            x: 0
            y: 0
            width:  bodyContainer.width - 460
            height: bodyContainer.height
            model: LibraryController.movies
            onMovieClicked: function (id) { LibraryController.selectMovie(id) }
        }

        MovieDetail {
            id: detailPane
            x: bodyContainer.width - 460
            y: 0
            width:  460
            height: bodyContainer.height
        }
    }

    // ---- Empty-state overlay --------------------------------------------
    // Shown only on a fresh library that doesn't have any movies yet.
    Pane {
        anchors.centerIn: parent
        visible: LibraryController.libraryOpen && LibraryController.movieCount === 0
        background: Rectangle { color: Material.background; radius: 6 }
        ColumnLayout {
            spacing: 12
            Label {
                text: qsTr("Library is empty")
                font.pixelSize: 22
                Layout.alignment: Qt.AlignHCenter
            }
            Label {
                text: qsTr("Import a DVD Profiler Collection.xml to populate it.")
                opacity: 0.7
                Layout.alignment: Qt.AlignHCenter
            }
            Button {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Import Collection.xml…")
                onClicked: importXmlDialog.open()
            }
        }
    }

    // ---- Dialogs ---------------------------------------------------------
    FileDialog {
        id: importXmlDialog
        title: qsTr("Import DVD Profiler Collection.xml")
        nameFilters: [qsTr("DVD Profiler XML (Collection.xml *.xml)")]
        fileMode: FileDialog.OpenFile
        // Begin (parse only) — the preview dialog then prompts for
        // confirmation before the actual DB write. Images dir defaults
        // to the user's configured one.
        onAccepted: LibraryController.beginImport(
            selectedFile,
            SettingsController.imagesDirectory)
    }

    // TMDb match picker — auto-shows when the controller has candidates.
    TmdbMatchDialog { }

    // Persistent settings editor — opened from the toolbar.
    SettingsDialog { id: settingsDialog }

    // Confirmation step between file-pick and DB-write.
    ImportPreviewDialog { }

    // Modal progress dialog. Visible whenever the controller has an
    // import running; auto-closes when the worker finishes.
    Dialog {
        id: importProgressDialog
        modal: true
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton
        title: qsTr("Importing…")
        visible: LibraryController.importInProgress
        implicitWidth: 460

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Label {
                Layout.fillWidth: true
                text: LibraryController.importStage
                wrapMode: Text.WordWrap
            }
            ProgressBar {
                Layout.fillWidth: true
                from:  0
                to:    Math.max(1, LibraryController.importTotal)
                value: LibraryController.importCurrent
                // Show an indeterminate animation during the XML-parsing
                // phase (importTotal is still 0 until the writer phase starts).
                indeterminate: LibraryController.importTotal === 0
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: LibraryController.importTotal > 0
                    ? qsTr("%1 / %2").arg(LibraryController.importCurrent)
                                     .arg(LibraryController.importTotal)
                    : ""
                opacity: 0.7
                font.pixelSize: 12
            }
        }
    }
}
