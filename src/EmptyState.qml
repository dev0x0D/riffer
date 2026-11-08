import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 14

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "♫"
            font.pixelSize: 68
            color: theme.accent
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Practice at your own tempo")
            font.pixelSize: 22
            font.bold: true
            color: "#f0f0f4"
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Open an MP3 or WAV to slow it down without changing the pitch.")
            font.pixelSize: 13
            color: "#8a8a92"
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            text: qsTr("Open file")
            highlighted: true
            onClicked: backend.open()
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("…or drop a file anywhere in this window")
            font.pixelSize: 11
            color: "#6d6d75"
        }
    }
}
