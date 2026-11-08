import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#b0000000"

    signal closed

    MouseArea {
        anchors.fill: parent
        onClicked: closed()
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 48)
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: helpColumn.implicitHeight + 48
            radius: Math.min(10, theme.radius)
            color: "#17171d"
            border.color: "#20ffffff"
            border.width: 1

            ColumnLayout {
                id: helpColumn
                anchors.fill: parent
                anchors.margins: 20
                spacing: 6

                Label {
                    text: qsTr("Keyboard shortcuts")
                    font.pixelSize: 15
                    font.bold: true
                    color: "#f0f0f4"
                    Layout.bottomMargin: 8
                }

                // A binding, not a model, so a language switch retranslates.
                readonly property var rows: [
                    { k: "Space", d: qsTr("Play from the start cursor; stop keeps the playhead where it stopped") },
                    { k: "[  ]", d: qsTr("Slower / faster by 5%") },
                    { k: "{  }", d: qsTr("Slower / faster by 1%") },
                    { k: "1 2 3 4", d: qsTr("Speed presets: 100%, 95%, 85%, 75% — the slider covers the rest") },
                    { k: "←  →", d: qsTr("Nudge the position by 2 s (Shift: 10 s) — while stopped it moves the start cursor") },
                    { k: "I  O", d: qsTr("Set selection start / end at the playhead") },
                    { k: "M", d: qsTr("Mark a section: press at the start, then the end") },
                    { k: "Shift+M", d: qsTr("Make a section from the selection") },
                    { k: "Tab", d: qsTr("Show or hide the sections panel") },
                    { k: "F / 0", d: qsTr("Zoom to selection / whole track") },
                    { k: "+  −", d: qsTr("Zoom in / out") },
                    { k: "Ctrl+wheel", d: qsTr("Zoom at the pointer") },
                    { k: "Home  End", d: qsTr("Jump to start / end") },
                    { k: "Ctrl+Z / Ctrl+Shift+Z", d: qsTr("Undo / redo a section edit") },
                    { k: "Ctrl+O", d: qsTr("Open a file") },
                    { k: "Esc", d: qsTr("Clear the selection, close panels and dialogs") },
                    { k: "Q", d: qsTr("Quit") }
                ]

                Repeater {
                    model: helpColumn.rows
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            text: modelData.k
                            color: theme.accent
                            font.pixelSize: 12
                            font.family: "monospace"
                            Layout.preferredWidth: 150
                        }
                        Label {
                            text: modelData.d
                            color: "#c8c8ce"
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Label {
                    Layout.topMargin: 10
                    text: qsTr("Click sets the play position; drag to loop a passage. The strip under the wave shows the whole track — click or drag it to move the view window.")
                    color: "#8a8a92"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Press Esc or click anywhere to close")
            color: "#8a8a92"
            font.pixelSize: 11
        }
    }
}
