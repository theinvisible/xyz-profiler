import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import xyz.profiler

// Modal candidate-picker. Bound directly to the controller's TMDb state:
// becomes visible whenever a search has produced candidates (or an error
// to surface), and disappears once the user picks one or cancels.
Dialog {
    id: root
    modal: true
    anchors.centerIn: parent
    title: qsTr("Match on TMDb")
    standardButtons: Dialog.Cancel
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    implicitWidth: Math.min(parent ? parent.width - 80 : 800, 720)
    implicitHeight: Math.min(parent ? parent.height - 80 : 720, 640)

    visible: LibraryController.tmdbSearching ||
             LibraryController.tmdbCandidates.length > 0 ||
             LibraryController.tmdbSearchError !== ""

    onRejected: LibraryController.clearTmdbCandidates()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            visible: LibraryController.tmdbSearching
            text: qsTr("Searching TMDb…")
            opacity: 0.7
        }

        Label {
            Layout.fillWidth: true
            visible: LibraryController.tmdbSearchError !== ""
            text: LibraryController.tmdbSearchError
            color: Material.color(Material.Red, Material.Shade400)
            wrapMode: Text.WordWrap
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !LibraryController.tmdbSearching &&
                     LibraryController.tmdbCandidates.length > 0
            clip: true
            spacing: 8
            model: LibraryController.tmdbCandidates

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                width: ListView.view.width
                height: 130
                onClicked: {
                    LibraryController.pickTmdbMatch(modelData.id);
                    root.close();
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 12

                    Image {
                        Layout.preferredWidth:  74
                        Layout.preferredHeight: 110
                        fillMode: Image.PreserveAspectFit
                        source: modelData.posterUrl || ""
                        asynchronous: true
                        cache: true
                        visible: source !== ""
                    }
                    Rectangle {
                        // Placeholder when no poster
                        Layout.preferredWidth:  74
                        Layout.preferredHeight: 110
                        color: Material.color(Material.Grey, Material.Shade800)
                        visible: !modelData.posterUrl
                        radius: 4
                        Label {
                            anchors.centerIn: parent
                            text: "—"
                            opacity: 0.5
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: modelData.title +
                                  (modelData.year > 0 ? "  (" + modelData.year + ")" : "")
                            font.bold: true
                            font.pixelSize: 14
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: modelData.originalTitle &&
                                     modelData.originalTitle !== modelData.title
                            text: modelData.originalTitle
                            opacity: 0.6
                            font.italic: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData.overview || ""
                            wrapMode: Text.WordWrap
                            elide: Text.ElideRight
                            maximumLineCount: 3
                            opacity: 0.85
                            font.pixelSize: 12
                        }
                        Label {
                            text: modelData.voteCount > 0
                                ? qsTr("★ %1  ·  %2 votes  ·  TMDb #%3")
                                    .arg(modelData.voteAverage.toFixed(1))
                                    .arg(modelData.voteCount)
                                    .arg(modelData.id)
                                : qsTr("TMDb #%1").arg(modelData.id)
                            opacity: 0.6
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !LibraryController.tmdbSearching &&
                     LibraryController.tmdbSearchError === "" &&
                     LibraryController.tmdbCandidates.length === 0
            text: qsTr("No matches found.")
            opacity: 0.7
        }

        // TMDb attribution — required by the TMDB API terms of use
        // (section 3). Goes at the foot of any view that actively
        // presents TMDB-sourced data.
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            Image {
                source: "qrc:/tmdb_logo.svg"
                sourceSize.height: 18
                fillMode: Image.PreserveAspectFit
                Layout.preferredHeight: 18
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
                opacity: 0.6
                font.pixelSize: 10
            }
        }
    }
}
