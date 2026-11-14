import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Metronome settings, hand-styled like SectionDialog: the Material chrome
// has proven unreliable on this stack, so the panel is plain dark surfaces,
// outline chips and pill buttons. Non-modal, so the click keeps running
// while the tempo, sound or volume is being adjusted.
Popup {
    id: popup
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 360
    topPadding: 12
    bottomPadding: 12
    leftPadding: 18
    rightPadding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: "#17171d"
        radius: 10
        border.color: "#26ffffff"
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 7

        RowLayout {
            spacing: 8
            Label {
                text: qsTr("Metronome")
                color: "#f0f0f4"
                font.pixelSize: 15
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            // On/off pill: the same accent-filled style as the dialog's Save.
            Rectangle {
                width: onOffLabel.implicitWidth + 24
                height: 26
                radius: 13
                color: backend.metronome.enabled ? theme.accent : "#1f1f27"
                border.color: backend.metronome.enabled ? theme.accent : "#3d3d47"
                border.width: 1
                Label {
                    id: onOffLabel
                    anchors.centerIn: parent
                    text: backend.metronome.enabled ? qsTr("On") : qsTr("Off")
                    color: backend.metronome.enabled ? theme.accentForeground : "#d8d8de"
                    font.pixelSize: 12
                    font.bold: backend.metronome.enabled
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.metronome.enabled = !backend.metronome.enabled
                }
            }
        }

        // Tempo: type a value, drag, or tap in rhythm.
        Label {
            text: qsTr("Tempo")
            color: "#8a8a92"
            font.pixelSize: 11
        }
        RowLayout {
            spacing: 8
            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 32
                radius: 6
                color: "#101014"
                border.color: bpmField.activeFocus ? theme.accent : "#26ffffff"
                border.width: 1
                TextInput {
                    id: bpmField
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignHCenter
                    color: "#e8e8ee"
                    font.pixelSize: 14
                    font.family: "monospace"
                    clip: true
                    selectByMouse: true
                    text: backend.metronome.bpm
                    validator: IntValidator { bottom: 30; top: 300 }
                    onEditingFinished: if (text !== "")
                        backend.metronome.bpm = parseInt(text, 10)
                }
            }
            Label {
                text: qsTr("BPM")
                color: "#8a8a92"
                font.pixelSize: 12
            }
            Slider {
                Layout.fillWidth: true
                from: 30
                to: 300
                stepSize: 1
                value: backend.metronome.bpm
                onMoved: backend.metronome.bpm = value
            }
        }

        RowLayout {
            spacing: 8
            Rectangle {
                width: tapLabel.implicitWidth + 32
                height: 32
                radius: 16
                color: tapMouse.pressed ? Qt.darker(theme.accent, 1.15)
                      : tapMouse.containsMouse ? "#2a2a33" : "#1f1f27"
                border.color: "#3d3d47"
                border.width: 1
                Label {
                    id: tapLabel
                    anchors.centerIn: parent
                    text: qsTr("Tap")
                    color: "#d8d8de"
                    font.pixelSize: 12
                    font.bold: true
                }
                MouseArea {
                    id: tapMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.metronome.tap()
                }
            }
            Label {
                text: qsTr("Tap in rhythm — the beats become the tempo")
                color: "#8a8a92"
                font.pixelSize: 11
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        // The click sound, one of the synthesized ones.
        Label {
            text: qsTr("Click sound")
            color: "#8a8a92"
            font.pixelSize: 11
        }
        Flow {
            spacing: 6
            Layout.fillWidth: true
            Repeater {
                model: [qsTr("Wood"), qsTr("Click"), qsTr("Beep"), qsTr("Block")]
                delegate: Rectangle {
                    width: soundLabel.implicitWidth + 22
                    height: 26
                    radius: 13
                    color: backend.metronome.sound === index ? "#2a2a33" : "#1f1f27"
                    border.color: backend.metronome.sound === index ? theme.accent : "#3d3d47"
                    border.width: 1
                    Label {
                        id: soundLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: backend.metronome.sound === index ? theme.accent : "#d8d8de"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.metronome.sound = index
                    }
                }
            }
        }

        // The time signature: an accent lands on beat one of each bar.
        Label {
            text: qsTr("Time signature")
            color: "#8a8a92"
            font.pixelSize: 11
        }
        Flow {
            spacing: 6
            Layout.fillWidth: true
            Repeater {
                model: [{ v: 1, l: "1" }, { v: 2, l: "2/4" }, { v: 3, l: "3/4" },
                        { v: 4, l: "4/4" }, { v: 5, l: "5/4" }, { v: 6, l: "6/4" },
                        { v: 7, l: "7/4" }]
                delegate: Rectangle {
                    width: barLabel.implicitWidth + 22
                    height: 26
                    radius: 13
                    color: backend.metronome.beatsPerBar === modelData.v ? "#2a2a33" : "#1f1f27"
                    border.color: backend.metronome.beatsPerBar === modelData.v ? theme.accent : "#3d3d47"
                    border.width: 1
                    Label {
                        id: barLabel
                        anchors.centerIn: parent
                        text: modelData.l
                        color: backend.metronome.beatsPerBar === modelData.v ? theme.accent : "#d8d8de"
                        font.pixelSize: 12
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: backend.metronome.beatsPerBar = modelData.v
                    }
                }
            }
        }

        // Click volume, separate from the track volume in the header.
        Label {
            text: qsTr("Click volume")
            color: "#8a8a92"
            font.pixelSize: 11
        }
        Slider {
            from: 0
            to: 1
            stepSize: 0.01
            value: backend.metronome.gain
            Layout.fillWidth: true
            onMoved: backend.metronome.gain = value
        }
    }
}
