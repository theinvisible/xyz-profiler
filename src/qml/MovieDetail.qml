import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

import xyz.profiler

Pane {
    id: pane
    implicitWidth: 460
    Material.background: Material.color(Material.Grey, Material.Shade900)

    // ---- Empty-selection state -------------------------------------------
    Label {
        anchors.centerIn: parent
        visible: !LibraryController.hasSelection
        text: qsTr("Select a movie")
        opacity: 0.5
        font.pixelSize: 16
    }

    // ---- Detail content --------------------------------------------------
    ScrollView {
        anchors.fill: parent
        visible: LibraryController.hasSelection
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: pane.availableWidth
            spacing: 14

            // ---- Title block ----------------------------------------------
            Label {
                text: LibraryController.selectedTitle
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                visible: LibraryController.selectedOriginalTitle !== "" &&
                         LibraryController.selectedOriginalTitle !== LibraryController.selectedTitle
                text: LibraryController.selectedOriginalTitle
                opacity: 0.7
                font.italic: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label {
                visible: LibraryController.selectedDistTrait !== ""
                text: LibraryController.selectedDistTrait
                opacity: 0.7
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Label {
                text: {
                    const parts = [];
                    if (LibraryController.selectedYear > 0)    parts.push(LibraryController.selectedYear);
                    if (LibraryController.selectedRuntime > 0) parts.push(LibraryController.selectedRuntime + " min");
                    if (LibraryController.selectedFormat)      parts.push(LibraryController.selectedFormat);
                    if (LibraryController.selectedRating)      parts.push(LibraryController.selectedRating);
                    if (LibraryController.selectedRatingAge > 0)
                        parts.push("age " + LibraryController.selectedRatingAge);
                    return parts.join("  ·  ");
                }
                opacity: 0.85
                Layout.fillWidth: true
            }

            // ---- Cover thumbnail ------------------------------------------
            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 240
                Layout.preferredHeight: 360
                fillMode: Image.PreserveAspectFit
                source: LibraryController.selectedCoverFrontUrl
                visible: source !== ""
                asynchronous: true
            }

            // ---- Loan badge ----------------------------------------------
            Rectangle {
                visible: LibraryController.selectedIsLoaned
                color: Material.color(Material.Red, Material.Shade800)
                radius: 6
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                Column {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2
                    Label { text: qsTr("Loaned out"); color: "white"; font.bold: true }
                    Label {
                        text: qsTr("To %1, due %2")
                            .arg(LibraryController.selectedLoanUser || qsTr("(no name)"))
                            .arg(LibraryController.selectedLoanDue  || qsTr("?"))
                        color: "white"; opacity: 0.9
                    }
                }
            }

            // ---- Overview -------------------------------------------------
            Label { text: qsTr("OVERVIEW");      visible: LibraryController.selectedOverview !== "";    font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedOverview; visible: LibraryController.selectedOverview !== ""; wrapMode: Text.WordWrap; Layout.fillWidth: true; textFormat: Text.RichText }

            // ---- Notes ---------------------------------------------------
            Label { text: qsTr("NOTES");         visible: LibraryController.selectedNotes !== "";       font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedNotes; visible: LibraryController.selectedNotes !== ""; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            // ---- Easter Eggs ---------------------------------------------
            Label { text: qsTr("EASTER EGGS");   visible: LibraryController.selectedEasterEggs !== "";  font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedEasterEggs; visible: LibraryController.selectedEasterEggs !== ""; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            // ---- Classification ------------------------------------------
            Label { text: qsTr("GENRES");        visible: LibraryController.selectedGenres.length > 0;  font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedGenres.join(", "); visible: LibraryController.selectedGenres.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            Label { text: qsTr("STUDIOS");       visible: LibraryController.selectedStudios.length > 0; font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedStudios.join(", "); visible: LibraryController.selectedStudios.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            // ---- Cast ----------------------------------------------------
            Label {
                text: qsTr("CAST (%1)").arg(LibraryController.selectedActors.length)
                visible: LibraryController.selectedActors.length > 0
                font.pixelSize: 11; font.bold: true; color: Material.accent
            }
            Repeater {
                model: LibraryController.selectedActors
                Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.role
                          ? modelData.name + "  —  " + modelData.role
                          : modelData.name
                    elide: Text.ElideRight
                    opacity: 0.9
                }
            }

            // ---- Crew ----------------------------------------------------
            Label {
                text: qsTr("CREW (%1)").arg(LibraryController.selectedCredits.length)
                visible: LibraryController.selectedCredits.length > 0
                font.pixelSize: 11; font.bold: true; color: Material.accent
            }
            Repeater {
                model: LibraryController.selectedCredits
                Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.creditType
                          ? modelData.name + "  —  " + modelData.creditType +
                            (modelData.role && modelData.role !== modelData.creditType
                                 ? " / " + modelData.role
                                 : "")
                          : modelData.name
                    elide: Text.ElideRight
                    opacity: 0.9
                }
            }

            // ---- Audio ---------------------------------------------------
            Label {
                text: qsTr("AUDIO (%1)").arg(LibraryController.selectedAudioTracks.length)
                visible: LibraryController.selectedAudioTracks.length > 0
                font.pixelSize: 11; font.bold: true; color: Material.accent
            }
            Repeater {
                model: LibraryController.selectedAudioTracks
                Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: [modelData.content, modelData.format, modelData.channels]
                          .filter(function(s) { return s; }).join("  ·  ")
                    opacity: 0.9
                }
            }

            // ---- Subtitles -----------------------------------------------
            Label { text: qsTr("SUBTITLES");     visible: LibraryController.selectedSubtitles.length > 0; font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedSubtitles.join(", "); visible: LibraryController.selectedSubtitles.length > 0; wrapMode: Text.WordWrap; Layout.fillWidth: true }

            // ---- Discs ---------------------------------------------------
            Label {
                text: qsTr("DISCS (%1)").arg(LibraryController.selectedDiscs.length)
                visible: LibraryController.selectedDiscs.length > 0
                font.pixelSize: 11; font.bold: true; color: Material.accent
            }
            Repeater {
                model: LibraryController.selectedDiscs
                Label {
                    required property var modelData
                    Layout.fillWidth: true
                    text: (modelData.descriptionSideA || qsTr("Disc")) +
                          (modelData.discIdSideA ? "  ·  ID " + modelData.discIdSideA : "") +
                          (modelData.labelSideA  ? "  ·  " + modelData.labelSideA  : "")
                    opacity: 0.9
                    elide: Text.ElideRight
                }
            }

            // ---- Technical -----------------------------------------------
            Label { text: qsTr("TECHNICAL"); font.pixelSize: 11; font.bold: true; color: Material.accent
                    visible: LibraryController.selectedAspectRatio !== "" ||
                             LibraryController.selectedDimensions  !== "" ||
                             LibraryController.selectedFeatures.length > 0 ||
                             LibraryController.selectedRegions.length > 0 }
            Label {
                visible: LibraryController.selectedAspectRatio !== "" ||
                         LibraryController.selectedDimensions  !== ""
                text: [LibraryController.selectedAspectRatio,
                       LibraryController.selectedDimensions,
                       LibraryController.selectedCaseType].filter(function(s) { return s; }).join("  ·  ")
                Layout.fillWidth: true
            }
            Label {
                visible: LibraryController.selectedRegions.length > 0
                text: qsTr("Regions: ") + LibraryController.selectedRegions.join(", ")
                Layout.fillWidth: true
            }
            Label {
                visible: LibraryController.selectedFeatures.length > 0
                text: qsTr("Features: ") + LibraryController.selectedFeatures.join(", ")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // ---- Purchase ------------------------------------------------
            Label { text: qsTr("PURCHASE"); font.pixelSize: 11; font.bold: true; color: Material.accent
                    visible: LibraryController.selectedPurchaseDate !== "" ||
                             LibraryController.selectedPurchasePrice !== "" }
            Label { visible: LibraryController.selectedPurchaseDate  !== ""; text: qsTr("Date: ")  + LibraryController.selectedPurchaseDate }
            Label { visible: LibraryController.selectedPurchasePrice !== ""; text: qsTr("Price: ") + LibraryController.selectedPurchasePrice }
            Label { visible: LibraryController.selectedPurchasePlace !== ""; text: qsTr("Place: ") + LibraryController.selectedPurchasePlace }

            // ---- Tags ----------------------------------------------------
            Label { text: qsTr("TAGS"); visible: LibraryController.selectedTags.length > 0; font.pixelSize: 11; font.bold: true; color: Material.accent }
            Label { text: LibraryController.selectedTags.join(", "); visible: LibraryController.selectedTags.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
