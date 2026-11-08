import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 400
    title: qsTr("Name the section")
    standardButtons: Dialog.Save | Dialog.Cancel

    // The Material ninepatch background renders unreliably here, so the
    // dialog carries its own frame in the app's panel palette.
    topPadding: 18
    bottomPadding: 14
    leftPadding: 20
    rightPadding: 20
    background: Rectangle {
        color: "#17171d"
        radius: 10
        border.color: "#26ffffff"
        border.width: 1
    }

    property string rangeText: ""
    property alias suggestion: nameField.text
    signal acceptedNamed(string name)

    onAboutToShow: nameField.selectAll()
    onOpened: nameField.forceActiveFocus()
    onAccepted: acceptedNamed(nameField.text)

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            text: dialog.rangeText
            color: "#8a8a92"
            font.pixelSize: 12
        }
        TextField {
            id: nameField
            placeholderText: qsTr("Section name")
            color: "#e8e8ee"
            font.pixelSize: 13
            selectByMouse: true
            background: Rectangle {
                implicitWidth: 200
                implicitHeight: 34
                color: "#101014"
                radius: 6
                border.color: nameField.activeFocus ? theme.accent : "#26ffffff"
                border.width: 1
            }
            onAccepted: dialog.accept()
        }
        // Quick English names; a click fills the field, Enter saves.
        Flow {
            spacing: 6
            Layout.fillWidth: true
            Repeater {
                model: ["Intro", "Riff", "Verse", "Pre-Chorus", "Chorus", "Bridge",
                        "Solo", "Breakdown", "Fill", "Groove", "Outro"]
                delegate: Button {
                    flat: true
                    leftPadding: 10
                    rightPadding: 10
                    bottomPadding: 4
                    topPadding: 4
                    font.pixelSize: 11
                    text: modelData
                    onClicked: {
                        nameField.text = modelData
                        nameField.forceActiveFocus()
                        nameField.selectAll()
                    }
                }
            }
        }
    }
}
