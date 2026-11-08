import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The section naming dialog, styled entirely with the app's own panel
// palette and hand-drawn controls: the Material chrome rendered unreliably
// (its background dropped out, the placeholder floated above the field and
// the footer buttons were unreadably dim).
Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 420
    standardButtons: Dialog.NoButton

    topPadding: 18
    bottomPadding: 16
    leftPadding: 20
    rightPadding: 20
    background: Rectangle {
        color: "#17171d"
        radius: 10
        border.color: "#26ffffff"
        border.width: 1
    }

    property string rangeText: ""
    property alias suggestion: nameInput.text
    signal acceptedNamed(string name)

    onAboutToShow: nameInput.selectAll()
    onOpened: nameInput.forceActiveFocus()
    onAccepted: acceptedNamed(nameInput.text)

    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: qsTr("Name the section")
            color: "#f0f0f4"
            font.pixelSize: 15
            font.bold: true
        }
        Label {
            text: dialog.rangeText
            color: "#8a8a92"
            font.pixelSize: 12
            font.family: "monospace"
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 38
            radius: 6
            color: "#101014"
            border.color: nameInput.activeFocus ? theme.accent : "#26ffffff"
            border.width: 1
            TextInput {
                id: nameInput
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                verticalAlignment: TextInput.AlignVCenter
                color: "#e8e8ee"
                font.pixelSize: 13
                clip: true
                selectByMouse: true
                onAccepted: dialog.accept()
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    visible: nameInput.text === "" && !nameInput.activeFocus
                    text: qsTr("Section name")
                    color: "#8a8a92"
                    font.pixelSize: 13
                }
            }
        }
        // Quick English names; a click fills the field, Enter saves.
        Flow {
            spacing: 6
            Layout.fillWidth: true
            Repeater {
                model: ["Intro", "Riff", "Verse", "Pre-Chorus", "Chorus", "Bridge",
                        "Solo", "Breakdown", "Fill", "Groove", "Outro"]
                delegate: Rectangle {
                    width: chipLabel.implicitWidth + 24
                    height: 26
                    radius: 13
                    color: chipMouse.containsMouse ? "#2a2a33" : "#1f1f27"
                    border.color: "#3d3d47"
                    border.width: 1
                    Label {
                        id: chipLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: "#d8d8de"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        id: chipMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            nameInput.text = modelData
                            nameInput.forceActiveFocus()
                            nameInput.selectAll()
                        }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            Rectangle {
                width: cancelLabel.implicitWidth + 28
                height: 32
                radius: 16
                color: cancelMouse.containsMouse ? "#2a2a33" : "#1f1f27"
                border.color: "#3d3d47"
                border.width: 1
                Label {
                    id: cancelLabel
                    anchors.centerIn: parent
                    text: qsTr("Cancel")
                    color: "#d8d8de"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.reject()
                }
            }
            Rectangle {
                width: saveLabel.implicitWidth + 28
                height: 32
                radius: 16
                color: saveMouse.containsMouse ? Qt.darker(theme.accent, 1.15) : theme.accent
                Label {
                    id: saveLabel
                    anchors.centerIn: parent
                    text: qsTr("Save")
                    color: theme.accentForeground
                    font.pixelSize: 12
                    font.bold: true
                }
                MouseArea {
                    id: saveMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: dialog.accept()
                }
            }
        }
    }
}
