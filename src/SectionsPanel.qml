import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The section list: click a row to play the section (the window closes,
// the boundaries highlight and the section loops), ✎ to rename, ✕ to
// delete (Ctrl+Z undoes).
Rectangle {
    id: panel
    color: "transparent"

    signal closeRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 8
            Layout.topMargin: 10
            spacing: 8
            Label {
                text: qsTr("Sections")
                font.pixelSize: 12
                font.bold: true
                color: "#c8c8ce"
            }
            Label {
                visible: backend.sections.length === 0
                text: qsTr("No sections yet. Press M at the start and the end of a passage to mark one.")
                font.pixelSize: 11
                color: "#8a8a92"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Item { Layout.fillWidth: true }
            ToolButton { text: "✕"; onClicked: panel.closeRequested() }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 4
            clip: true
            model: backend.sections
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                width: list.width
                height: 46
                property bool editing: false
                // A plain Rectangle defaults to white: keep the rows on the
                // dark panel, with a subtle hover so the click target reads.
                color: rowArea.containsMouse ? "#1b1b23" : "transparent"

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        backend.playSection(index) // highlights boundaries, loops, plays
                        panel.closeRequested()
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 6
                    spacing: 10

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        TextField {
                            visible: editing
                            Layout.fillWidth: true
                            text: modelData.name
                            font.pixelSize: 13
                            color: "#e8e8ee"
                            onAccepted: {
                                backend.renameSection(index, text)
                                editing = false
                            }
                            onActiveFocusChanged: if (!activeFocus && editing)
                                editing = false
                        }
                        Label {
                            visible: !editing
                            text: modelData.name
                            font.pixelSize: 13
                            color: "#e8e8ee"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Label {
                            text: backend.formatTime(modelData.start, false) + " – "
                                  + backend.formatTime(modelData.end, false)
                            font.pixelSize: 11
                            color: "#8a8a92"
                        }
                    }

                    ToolButton {
                        visible: rowArea.containsMouse
                        text: "✎"
                        font.pixelSize: 12
                        ToolTip.visible: hovered
                        ToolTip.delay: 600
                        ToolTip.text: qsTr("Rename this section")
                        onClicked: editing = true
                    }
                    ToolButton {
                        text: "✕"
                        font.pixelSize: 12
                        ToolTip.visible: hovered
                        ToolTip.delay: 600
                        ToolTip.text: qsTr("Delete this section")
                        onClicked: backend.deleteSection(index)
                    }
                }
            }
        }
    }
}
