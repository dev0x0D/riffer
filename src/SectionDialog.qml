import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    width: 380
    title: qsTr("Name the section")
    standardButtons: Dialog.Save | Dialog.Cancel

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
            onAccepted: dialog.accept()
        }
    }
}
