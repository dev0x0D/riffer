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
        // The compact window is wide but short, so the shortcuts run in two
        // columns inside one card and the card stays within the height.
        width: Math.min(1000, parent.width - 48)
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: helpColumn.implicitHeight + 28
            radius: Math.min(10, theme.radius)
            color: "#17171d"
            border.color: "#20ffffff"
            border.width: 1

            ColumnLayout {
                id: helpColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 14
                spacing: 6

                Label {
                    text: qsTr("Keyboard shortcuts")
                    font.pixelSize: 15
                    font.bold: true
                    color: "#f0f0f4"
                }

                // A binding, not a model, so a language switch retranslates.
                readonly property var rows: [
                    { k: "Space", d: qsTr("Play from the start cursor; stop keeps the playhead where it stopped") },
                    { k: "|<", d: qsTr("Back to start: cursors to zero; if playing, it keeps playing from zero") },
                    { k: "[  ]", d: qsTr("Slower / faster by 5%") },
                    { k: "{  }", d: qsTr("Slower / faster by 1%") },
                    { k: "1 2 3 4", d: qsTr("Speed presets: 100%, 90%, 80%, 70% — the slider covers the rest") },
                    { k: "←  →", d: qsTr("Nudge the start cursor by 0.5 s (Shift: 5 s)") },
                    { k: "↑  ↓", d: qsTr("Volume up / down by 5%") },
                    { k: "B", d: qsTr("Turn the metronome on or off (double-click its chip for tempo, sound and size)") },
                    { k: "I  O", d: qsTr("Set selection start / end at the playhead") },
                    { k: "M", d: qsTr("Mark a section: press at the start, then the end") },
                    { k: "Shift+M", d: qsTr("Make a section from the selection") },
                    { k: "Tab", d: qsTr("Show or hide the sections window") },
                    { k: "F / 0", d: qsTr("Zoom to selection / whole track") },
                    { k: "+  −", d: qsTr("Zoom in / out") },
                    { k: "Ctrl+wheel", d: qsTr("Zoom at the pointer") },
                    { k: "Home  End", d: qsTr("Jump to start / end") },
                    { k: "Ctrl+Z / Ctrl+Shift+Z", d: qsTr("Undo / redo a section edit") },
                    { k: "Ctrl+O", d: qsTr("Open a file") },
                    { k: "Esc", d: qsTr("Clear the selection, close panels and dialogs") },
                    { k: "Q", d: qsTr("Quit") }
                ]
                // The left column takes the first half, the right the rest.
                readonly property int columnBreak: Math.ceil(rows.length / 2)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 28

                    Repeater {
                        model: 2
                        delegate: ColumnLayout {
                            id: column
                            required property int index
                            Layout.fillWidth: true
                            spacing: 4

                            Repeater {
                                model: column.index === 0
                                       ? helpColumn.rows.slice(0, helpColumn.columnBreak)
                                       : helpColumn.rows.slice(helpColumn.columnBreak)
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 10
                                    Label {
                                        text: modelData.k
                                        color: theme.accent
                                        font.pixelSize: 11
                                        font.family: "monospace"
                                        Layout.preferredWidth: 140
                                    }
                                    Label {
                                        text: modelData.d
                                        color: "#c8c8ce"
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.topMargin: 2
                    text: qsTr("Click sets the play position; drag to loop a passage. Esc or a click anywhere closes this.")
                    color: "#8a8a92"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
