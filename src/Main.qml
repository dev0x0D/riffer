import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Riffer 1.0

ApplicationWindow {
    id: win
    width: 1100
    height: 360
    minimumWidth: 860
    minimumHeight: 360
    visible: true
    title: backend.ready || backend.loading ? qsTr("%1 — Riffer").arg(backend.fileName) : "Riffer"
    color: "#101014"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    // Keys that type into fields must not fire application shortcuts.
    property bool typing: activeFocusItem !== null
                          && (activeFocusItem instanceof TextInput
                              || activeFocusItem instanceof TextArea)
    property double viewStart: 0
    property double viewEnd: 1

    // The sections panel needs real height; the compact window grows to
    // give it room and shrinks back once it is closed.
    function toggleSectionsPanel() {
        const showing = !sectionsPanel.visible
        sectionsPanel.visible = showing
        height += showing ? sectionsPanel.Layout.preferredHeight
                          : -sectionsPanel.Layout.preferredHeight
    }

    function span() { return Math.max(0.001, viewEnd - viewStart) }
    function clampView() {
        if (!backend.ready)
            return
        const sp = Math.min(Math.max(span(), 0.005), backend.duration)
        const s = Math.min(Math.max(0, viewStart), Math.max(0, backend.duration - sp))
        viewStart = s
        viewEnd = s + sp
    }
    function fitAll() {
        if (backend.ready) {
            viewStart = 0
            viewEnd = backend.duration
        }
    }
    function timeAt(px) { return viewStart + px / Math.max(1, waveBox.width) * span() }
    function zoomAt(px, factor) {
        if (!backend.ready)
            return
        const t = timeAt(px)
        const sp = Math.min(Math.max(span() * factor, 0.005), backend.duration)
        viewStart = t - px / Math.max(1, waveBox.width) * sp
        viewEnd = viewStart + sp
        clampView()
    }
    function centerOn(t) {
        const sp = span()
        viewStart = t - sp / 2
        // viewEnd must move with viewStart: clampView keeps the span of the
        // pair it sees, and a half-updated pair would silently rezoom.
        viewEnd = viewStart + sp
        clampView()
    }
    function followPlayhead() {
        if (!backend.playing || !backend.ready)
            return
        const sp = span()
        if (backend.playhead > viewEnd - sp * 0.08) {
            viewStart = backend.playhead - sp * 0.08
            viewEnd = viewStart + sp
            clampView()
        } else if (backend.playhead < viewStart) {
            viewStart = Math.max(0, backend.playhead - sp * 0.08)
            viewEnd = viewStart + sp
            clampView()
        }
    }
    function handleEsc() {
        if (sectionDialog.opened)
            return // the dialog rejects itself
        if (helpVisible) {
            helpVisible = false
            return
        }
        if (sectionsPanel.visible) {
            sectionsPanel.visible = false
            height -= sectionsPanel.Layout.preferredHeight
            return
        }
        if (backend.sectionPending) {
            backend.cancelSection()
            return
        }
        backend.clearSelection()
    }

    // ----------------------------------------------------------- shortcuts ---
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing; onActivated: backend.togglePlay() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; onActivated: backend.open() }
    Shortcut { sequence: "["; enabled: !win.typing; onActivated: backend.nudgeSpeed(-0.05) }
    Shortcut { sequence: "]"; enabled: !win.typing; onActivated: backend.nudgeSpeed(0.05) }
    Shortcut { sequence: "{"; enabled: !win.typing; onActivated: backend.nudgeSpeed(-0.01) }
    Shortcut { sequence: "}"; enabled: !win.typing; onActivated: backend.nudgeSpeed(0.01) }
    Shortcut { sequence: "1"; enabled: !win.typing; onActivated: backend.setSpeed(1.0) }
    Shortcut { sequence: "2"; enabled: !win.typing; onActivated: backend.setSpeed(0.9) }
    Shortcut { sequence: "3"; enabled: !win.typing; onActivated: backend.setSpeed(0.8) }
    Shortcut { sequence: "4"; enabled: !win.typing; onActivated: backend.setSpeed(0.7) }
    Shortcut { sequence: "Left"; enabled: !win.typing; onActivated: backend.seekRelative(-0.5) }
    Shortcut { sequence: "Right"; enabled: !win.typing; onActivated: backend.seekRelative(0.5) }
    Shortcut { sequence: "Shift+Left"; enabled: !win.typing; onActivated: backend.seekRelative(-5) }
    Shortcut { sequence: "Shift+Right"; enabled: !win.typing; onActivated: backend.seekRelative(5) }
    Shortcut { sequence: "Up"; enabled: !win.typing; onActivated: backend.nudgeVolume(0.05) }
    Shortcut { sequence: "Down"; enabled: !win.typing; onActivated: backend.nudgeVolume(-0.05) }
    Shortcut { sequence: "Home"; enabled: !win.typing; onActivated: { backend.setCursor(0); win.centerOn(0) } }
    Shortcut { sequence: "End"; enabled: !win.typing; onActivated: { backend.setCursor(Math.max(0, backend.duration - 0.05)); win.centerOn(backend.cursor) } }
    Shortcut {
        sequence: "I"; enabled: !win.typing
        onActivated: backend.select(backend.playhead,
                                    backend.hasSelection ? backend.selectionEnd : backend.duration)
    }
    Shortcut {
        sequence: "O"; enabled: !win.typing
        onActivated: backend.select(backend.hasSelection ? backend.selectionStart : 0,
                                    backend.playhead)
    }
    Shortcut { sequence: "M"; enabled: !win.typing; onActivated: backend.beginSection() }
    Shortcut { sequence: "Shift+M"; enabled: !win.typing; onActivated: backend.sectionFromSelection() }
    Shortcut {
        sequence: "F"; enabled: !win.typing
        onActivated: if (backend.hasSelection) {
            win.viewStart = backend.selectionStart
            win.viewEnd = backend.selectionEnd
            win.clampView()
        }
    }
    Shortcut { sequence: "0"; enabled: !win.typing; onActivated: win.fitAll() }
    Shortcut { sequence: "+"; enabled: !win.typing; onActivated: win.zoomAt(waveBox.width / 2, 0.6) }
    Shortcut { sequence: "="; enabled: !win.typing; onActivated: win.zoomAt(waveBox.width / 2, 0.6) }
    Shortcut { sequence: "-"; enabled: !win.typing; onActivated: win.zoomAt(waveBox.width / 2, 1 / 0.6) }
    Shortcut { sequence: "Tab"; enabled: !win.typing; onActivated: toggleSectionsPanel() }
    Shortcut { sequence: "Esc"; enabled: true; onActivated: win.handleEsc() }
    Shortcut { sequence: "Ctrl+Z"; enabled: !win.typing; onActivated: backend.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !win.typing; onActivated: backend.redo() }
    Shortcut { sequence: "?"; enabled: !win.typing; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: !win.typing; onActivated: Qt.quit() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ------------------------------------------------------ header ---
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 10
                Layout.topMargin: 10
                Layout.bottomMargin: 6
                spacing: 10

                ColumnLayout {
                    spacing: 1
                    Label {
                        text: backend.ready || backend.loading ? backend.fileName : qsTr("Riffer")
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: 520
                    }
                    Label {
                        visible: backend.ready
                        text: backend.formatTime(backend.duration, false)
                              + "  ·  " + qsTr("%n sections", "", backend.sections.length)
                        color: "#8a8a92"
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
                // Volume in the corner: always within reach, and visibly
                // separate from the speed slider in the transport bar.
                RowLayout {
                    spacing: 6
                    Label { text: qsTr("Vol"); font.pixelSize: 11; color: "#8a8a92" }
                    Slider {
                        id: volumeSlider
                        from: 0
                        to: 1
                        stepSize: 0.01
                        value: backend.volume
                        Layout.preferredWidth: 90
                        onMoved: backend.setVolume(value)
                    }
                }
                Button {
                    flat: true
                    text: qsTr("Open")
                    onClicked: backend.open()
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Open an audio file (Ctrl+O)")
                }
                ToolButton {
                    text: "?"
                    font.pixelSize: 14
                    onClicked: win.helpVisible = !win.helpVisible
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: qsTr("Keyboard shortcuts (?)")
                }
            }

            // ------------------------------------------------ error banner ---
            Rectangle {
                visible: backend.error !== ""
                Layout.fillWidth: true
                Layout.margins: 12
                Layout.preferredHeight: 38
                radius: Math.min(6, theme.radius)
                color: "#3a1d20"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 4
                    spacing: 8
                    Label {
                        text: backend.error
                        color: "#ffb4b0"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                    ToolButton { text: "✕"; onClicked: backend.clearError() }
                }
            }

            // --------------------------------------------------- the wave  ---
            Item {
                id: waveBox
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                WaveformView {
                    id: wave
                    anchors.fill: parent
                    track: backend
                    viewStart: win.viewStart
                    viewEnd: win.viewEnd
                    accent: theme.accent
                    accentForeground: theme.accentForeground
                    radius: Math.min(6, theme.radius)
                }

                MouseArea {
                    id: waveMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    // 0 none, 1 drawing a selection, 2/3 dragging an edge, 4 pan
                    property int mode: 0
                    property double anchor: 0
                    property real panX: 0
                    property bool moved: false

                    cursorShape: {
                        if (!containsMouse)
                            return Qt.ArrowCursor
                        if (mode === 2 || mode === 3)
                            return Qt.SizeHorCursor
                        if (backend.hasSelection) {
                            const xs = (backend.selectionStart - win.viewStart) / win.span() * width
                            const xe = (backend.selectionEnd - win.viewStart) / win.span() * width
                            if (Math.abs(mouseX - xs) < 7 || Math.abs(mouseX - xe) < 7)
                                return Qt.SizeHorCursor
                        }
                        return Qt.CrossCursor
                    }

                    onPressed: mouse => {
                        moved = false
                        if (mouse.button === Qt.MiddleButton) {
                            mode = 4
                            panX = mouse.x
                            return
                        }
                        const t = win.timeAt(mouse.x)
                        if (backend.hasSelection) {
                            const xs = (backend.selectionStart - win.viewStart) / win.span() * width
                            const xe = (backend.selectionEnd - win.viewStart) / win.span() * width
                            if (Math.abs(mouse.x - xs) < 7) {
                                mode = 2
                                return
                            }
                            if (Math.abs(mouse.x - xe) < 7) {
                                mode = 3
                                return
                            }
                        }
                        mode = 1
                        anchor = t
                    }
                    onPositionChanged: mouse => {
                        if (!pressed)
                            return
                        if (mode === 4) {
                            const dx = mouse.x - panX
                            const shift = -dx / Math.max(1, width) * win.span()
                            win.viewStart += shift
                            win.viewEnd += shift
                            panX = mouse.x
                            win.clampView()
                            return
                        }
                        const t = win.timeAt(mouse.x)
                        if (mode === 1) {
                            if (moved || Math.abs(t - anchor) > 0.004) {
                                moved = true
                                backend.select(Math.min(anchor, t), Math.max(anchor, t))
                            }
                        } else if (mode === 2) {
                            backend.select(Math.min(t, backend.selectionEnd - 0.06),
                                           backend.selectionEnd)
                        } else if (mode === 3) {
                            backend.select(backend.selectionStart,
                                           Math.max(t, backend.selectionStart + 0.06))
                        }
                    }
                    onReleased: mouse => {
                        if (mode === 1 && !moved)
                            backend.setCursor(win.timeAt(mouse.x))
                        mode = 0
                    }
                    onDoubleClicked: backend.clearSelection()
                    onWheel: wheel => {
                        if (wheel.modifiers & Qt.ControlModifier) {
                            win.zoomAt(wheel.x, wheel.angleDelta.y > 0 ? 0.8 : 1.25)
                        } else {
                            const sp = win.span()
                            const shift = (wheel.angleDelta.x - wheel.angleDelta.y) / 120 * sp * 0.15
                            win.viewStart += shift
                            win.viewEnd += shift
                            win.clampView()
                        }
                    }
                }

                EmptyState {
                    anchors.fill: parent
                    visible: !backend.ready && !backend.loading
                }

                Rectangle {
                    visible: backend.loading
                    anchors.centerIn: parent
                    width: 280
                    height: 92
                    radius: Math.min(10, theme.radius)
                    color: "#17171d"
                    border.color: "#20ffffff"
                    border.width: 1
                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 10
                        BusyIndicator {
                            Layout.alignment: Qt.AlignHCenter
                            running: backend.loading
                        }
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            text: backend.progress > 0
                                  ? qsTr("Decoding… %1%").arg(
                                        Number(Math.round(backend.progress * 100))
                                        .toLocaleString(Qt.locale(), "f", 0))
                                  : qsTr("Decoding…")
                            color: "#c8c8ce"
                        }
                    }
                }
            }

            // -------------------------------------- overview strip ---
            OverviewBar {
                id: overview
                visible: backend.ready
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                track: backend
                viewStart: win.viewStart
                viewEnd: win.viewEnd
                accent: theme.accent
                // Only ever moves the window: the span is captured first and
                // both edges move together, so the zoom stays as the wave's
                // zoom controls set it.
                onViewPanRequested: start => {
                    const sp = win.span()
                    win.viewStart = start
                    win.viewEnd = start + sp
                    win.clampView()
                }
            }

            // ---------------------------------------------------- sections  ---
            SectionsPanel {
                id: sectionsPanel
                visible: false
                Layout.fillWidth: true
                Layout.preferredHeight: 170
            }

            // --------------------------------------------------- transport  ---
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: "#15151a"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    Rectangle {
                        width: 46
                        height: 46
                        radius: 23
                        color: theme.accent
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.togglePlay()
                        }
                        Label {
                            anchors.centerIn: parent
                            text: backend.playing ? "■" : "▶"
                            color: theme.accentForeground
                            font.pixelSize: 17
                        }
                    }

                    RowLayout {
                        spacing: 6
                        Label {
                            text: backend.formatTime(backend.playhead, true)
                            font.pixelSize: 18
                            font.bold: true
                            color: "#f0f0f4"
                            font.family: "monospace"
                        }
                        Label {
                            text: "/ " + backend.formatTime(backend.duration, false)
                            font.pixelSize: 12
                            color: "#8a8a92"
                            font.family: "monospace"
                        }
                    }

                    Rectangle {
                        visible: backend.sectionPending
                        height: 26
                        radius: 13
                        width: pendingLabel.implicitWidth + 30
                        color: "#14ffffff"
                        Label {
                            id: pendingLabel
                            anchors.centerIn: parent
                            font.pixelSize: 11
                            color: "#d8d8de"
                            text: qsTr("Section start set — press M again at the end")
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Speed presets and fine control.
                    Row {
                        spacing: 4
                        Repeater {
                            model: [0.5, 0.6, 0.7, 0.8, 0.9, 1.0]
                            delegate: Button {
                                flat: true
                                font.pixelSize: 11
                                enabled: backend.ready
                                text: Number(Math.round(modelData * 100)).toLocaleString(Qt.locale(), "f", 0) + "%"
                                highlighted: Math.abs(backend.speed - modelData) < 0.005
                                onClicked: backend.setSpeed(modelData)
                            }
                        }
                    }
                    RowLayout {
                        spacing: 4
                        Label {
                            text: Number(Math.round(backend.speed * 100)).toLocaleString(Qt.locale(), "f", 0) + "%"
                            color: theme.accent
                            font.bold: true
                            font.pixelSize: 16
                            Layout.preferredWidth: 58
                            horizontalAlignment: Text.AlignHCenter
                        }
                        Slider {
                            id: speedSlider
                            from: 0.2
                            to: 1.5
                            stepSize: 0.01
                            value: backend.speed
                            enabled: backend.ready
                            Layout.preferredWidth: 170
                            onMoved: backend.setSpeed(value)
                        }
                    }

                    // Volume lives in the header corner, out of the speed
                    // slider's way; this bar is about time and speed.
                }
            }
        }
    }

    // Sliders keep programmatic and dragged values in sync this way, because
    // a drag replaces the declarative binding on `value`.
    Connections {
        target: backend
        function onSpeedChanged() { speedSlider.value = backend.speed }
        function onVolumeChanged() { volumeSlider.value = backend.volume }
        function onPlayheadChanged() { win.followPlayhead() }
        function onTrackChanged() { win.fitAll() }
        function onCenterRequested(sec) { win.centerOn(sec) }
        function onNameSectionRequested(start, end, suggestion) {
            sectionDialog.rangeText = backend.formatTime(start, false) + " – "
                                      + backend.formatTime(end, false)
            sectionDialog.suggestion = suggestion
            sectionDialog.open()
        }
    }

    SectionDialog {
        id: sectionDialog
        onAcceptedNamed: name => backend.confirmSection(name)
        onRejected: backend.cancelSection()
    }

    HelpOverlay {
        anchors.fill: parent
        visible: win.helpVisible
        onClosed: win.helpVisible = false
    }

    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (drop.urls.length > 0)
                backend.openPath(drop.urls[0])
        }
    }
}
