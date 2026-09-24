// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The persistent bottom play bar: the selected instance on the left with a
 * picker to change it, the big Play button anchored at the right so it sits
 * in the window's bottom-right corner. Replaces the library's old
 * "Continue playing" hero -- one always-there place to press Play instead
 * of one that only showed up on the Library page.
 *
 * This is a genuine row in Main.qml's own layout, not a floating overlay:
 * StackLayout's page area shrinks by exactly this bar's height whenever it
 * is visible, so it can never cover a page's content, on any page -- including
 * ones this change does not own the QML of (Discover).
 *
 * `rowModel` is the shell's heroModel, freed up by removing the library's
 * own hero card; Main.qml points its instanceId at whichever instance this
 * bar should show (see its own dockInstanceId).
 */
Item {
    id: root

    // Single-row model (QmlShell.heroModel) already bound to the instance
    // this bar shows -- see Main.qml's dockInstanceId.
    property var rowModel: null
    // The full library list, for the picker's search field.
    property var instanceModel: null
    // Most-recently-played first, for the picker's default listing.
    property var recentModel: null
    // Main.qml's selectedId, so the picker can highlight the current row.
    property string selectedId: ""

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal cancelRequested(string id)

    // Every decorative loop stops when the window is not the active one and
    // when the user has asked for less motion.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive
                                          && !SettingsStore.bool("UiReduceMotion")

    implicitHeight: 84

    activeFocusOnTab: true
    Keys.onReturnPressed: root.tryLaunch()
    Keys.onSpacePressed: root.tryLaunch()
    function tryLaunch() {
        if (!root.hasInstance || root.launching)
            return
        if (root.dockRunning)
            root.stopRequested(root.dockInstanceId)
        else if (root.dockCanLaunch)
            root.launchRequested(root.dockInstanceId)
    }

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Play bar")

    // For a dev-route snapshot ("picker") that needs the instance picker
    // open without a real click.
    function openPicker() { picker.open() }

    // -- The current row, read out of rowModel's one row --------------------
    // Same idiom as InstancePage's header ContinueCard/InstanceOverviewTab
    // sync: a role model with 0-1 rows has no properties of its own to bind
    // to directly, so a throwaway delegate reads its roles into plain
    // properties everything else here can use.
    property string dockInstanceId: ""
    property string dockName: ""
    property string dockIconKey: ""
    property bool dockRunning: false
    property bool dockCanLaunch: false
    property var dockTotalTimePlayed: 0
    property string dockGameVersion: ""
    property string dockLoader: ""
    property color dockIconTint: Theme.palette.textTertiary
    property string dockCoverImage: ""
    property string dockLaunchStatus: ""
    property real dockLaunchProgress: -1

    readonly property bool hasInstance: root.dockInstanceId.length > 0
    readonly property bool launching: root.dockLaunchStatus.length > 0

    Repeater {
        model: root.rowModel
        delegate: Item {
            id: probe
            required property string instanceId
            required property string name
            required property string iconKey
            required property bool isRunning
            required property bool canLaunch
            required property var totalTimePlayed
            required property string gameVersion
            required property string loader
            required property color iconTint
            required property string coverImage
            required property string launchStatus
            required property real launchProgress
            visible: false
            width: 0
            height: 0

            function sync() {
                root.dockInstanceId = probe.instanceId
                root.dockName = probe.name
                root.dockIconKey = probe.iconKey
                root.dockRunning = probe.isRunning
                root.dockCanLaunch = probe.canLaunch
                root.dockTotalTimePlayed = probe.totalTimePlayed
                root.dockGameVersion = probe.gameVersion
                root.dockLoader = probe.loader
                root.dockIconTint = probe.iconTint
                root.dockCoverImage = probe.coverImage
                root.dockLaunchStatus = probe.launchStatus
                root.dockLaunchProgress = probe.launchProgress
            }
            Component.onCompleted: sync()
            onNameChanged: sync()
            onIconKeyChanged: sync()
            onIsRunningChanged: sync()
            onCanLaunchChanged: sync()
            onTotalTimePlayedChanged: sync()
            onGameVersionChanged: sync()
            onLoaderChanged: sync()
            onIconTintChanged: sync()
            onCoverImageChanged: sync()
            onLaunchStatusChanged: sync()
            onLaunchProgressChanged: sync()
        }
    }
    // No row at all (library empty, or the model has not been pointed at an
    // id yet): fall back to blank rather than stale leftovers.
    onRowModelChanged: if (!rowModel) { dockInstanceId = ""; dockName = "" }

    // -- Backdrop: the selected instance's own screenshot, darkened --------
    // The art itself spans the whole bar (CoverArt's own tinted plate
    // fallback covers an instance with no screenshot yet); scrim: "none"
    // here because CoverArt's own horizontal scrim jumps to 62% opacity by
    // the 45% mark, which read as one flat dark block on the left and a
    // separate bright picture on the right rather than one continuous
    // surface. The gradient below fades across the full width instead, so
    // the art is never fully hidden, only ever darkened.
    CoverArt {
        id: backdrop
        anchors.fill: parent
        // Square corners: this bar sits flush against the window's own
        // bottom and side edges, where a rounded photo would look cut off
        // rather than intentional. radius: 0 also skips CoverArt's corner
        // mask entirely, so there is no matte colour to get right here.
        radius: 0
        source: root.dockCoverImage
        tint: root.dockIconTint
        iconKey: ""
        scrim: "none"
    }
    Rectangle {
        id: scrimRect
        anchors.fill: parent
        // GradientStop's own `parent` is the Gradient, not this Rectangle --
        // referencing this id directly instead of `parent` is what a
        // GradientStop child actually needs to reach a property declared
        // out here.
        readonly property color scrimBase: Theme.media.scrim
        // Light-mode's photo-less plate is intentionally pale (CoverArt's
        // own recipe), and this bar's text is always light regardless of
        // theme -- easing the scrim back there would break that contrast.
        // Only dark mode's plate is already dark enough on its own that the
        // full-strength scrim on top reads as flat black.
        readonly property real strength: (!backdrop.hasPhoto && Theme.dark) ? 0.5 : 1.0
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(scrimRect.scrimBase.r, scrimRect.scrimBase.g, scrimRect.scrimBase.b, 0.80 * scrimRect.strength) }
            GradientStop { position: 0.35; color: Qt.rgba(scrimRect.scrimBase.r, scrimRect.scrimBase.g, scrimRect.scrimBase.b, 0.55 * scrimRect.strength) }
            GradientStop { position: 0.7; color: Qt.rgba(scrimRect.scrimBase.r, scrimRect.scrimBase.g, scrimRect.scrimBase.b, 0.28 * scrimRect.strength) }
            GradientStop { position: 1.0; color: Qt.rgba(scrimRect.scrimBase.r, scrimRect.scrimBase.g, scrimRect.scrimBase.b, 0.14 * scrimRect.strength) }
        }
    }
    // With no screenshot, CoverArt's own fallback plate is already dark in
    // dark mode (design-plan.md's tint recipe) -- the full-strength scrim
    // above piled on top of it is what read as flat black rather than a
    // designed plate. The same block-grid wash the other chrome-only
    // screens use (see AmbientPattern.qml) gives it a little texture
    // instead. Declared after scrimRect (not before) so its dots paint on
    // top of the scrim rather than being buried under it.
    AmbientPattern {
        anchors.fill: parent
        visible: !backdrop.hasPhoto
        tint: Theme.media.text
        strength: 0.08
    }
    // A thin highlight along the top edge instead of a plain divider --
    // the bar reads as one raised surface rather than a hairline-separated
    // strip.
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Qt.rgba(1, 1, 1, 0.10)
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl + Theme.space.xs
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.md

        AbstractButton {
            id: pickerButton
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: pickerRow.implicitWidth + Theme.space.md * 2
            implicitHeight: Theme.control.heightLg + Theme.space.sm
            hoverEnabled: true
            Accessible.name: root.hasInstance ? qsTr("Change instance: %1").arg(root.dockName) : qsTr("Choose an instance")

            onClicked: picker.visible ? picker.close() : picker.open()

            background: Rectangle {
                radius: Theme.radius.lg
                color: pickerButton.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }

            contentItem: RowLayout {
                id: pickerRow
                spacing: Theme.space.sm

                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: Theme.radius.md
                    color: Format.shade(root.dockIconTint, Theme.dark ? 0.30 : 0.86, 0.55)
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.10)

                    Image {
                        anchors.centerIn: parent
                        width: 30
                        height: 30
                        visible: root.hasInstance
                        source: root.dockIconKey.length > 0 ? "image://instanceicon/" + root.dockIconKey : ""
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: !root.hasInstance
                        iconName: "library"
                        size: Theme.icon.md
                        color: Theme.media.textSecondary
                    }
                }

                // A ColumnLayout, not a plain Column with a fixed width: a
                // short instance name should sit close to its icon with the
                // chevron right after it, not leave a dead gap up to a
                // hard-coded column width -- Layout.maximumWidth below caps
                // growth (eliding) without forcing short names to stretch.
                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1

                    Text {
                        Layout.maximumWidth: 190
                        text: root.hasInstance ? root.dockName : qsTr("Choose an instance")
                        elide: Text.ElideRight
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.bodyStrong.pixelSize
                        font.weight: Theme.type.bodyStrong.weight
                    }
                    Text {
                        id: subtitle
                        Layout.maximumWidth: 190
                        visible: root.hasInstance && subtitle.text.length > 0
                        text: [Format.versionLine(root.dockLoader, root.dockGameVersion),
                               Format.playTime(root.dockTotalTimePlayed)].filter(s => s.length > 0).join(" · ")
                        elide: Text.ElideRight
                        color: Theme.media.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }

                MeshIcon {
                    Layout.alignment: Qt.AlignVCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: Theme.media.textSecondary
                    rotation: picker.visible ? 180 : 0
                    Behavior on rotation { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
            }
        }

        Item { Layout.fillWidth: true }

        // Ready / launching / running -- three mutually exclusive faces,
        // cross-fading into each other rather than swapping abruptly, so a
        // click's burst on the Play button visibly "morphs" into the
        // progress display once the real launch status arrives.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Math.max(readyFace.implicitWidth, launchingFace.implicitWidth, runningFace.implicitWidth)
            Layout.preferredHeight: Theme.control.heightLg + 6

            PlayButton {
                id: readyFace
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                hero: true
                round: false
                size: Theme.control.heightLg + 6
                enabled: root.hasInstance && root.dockCanLaunch
                opacity: root.hasInstance && !root.launching && !root.dockRunning ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                onClicked: root.launchRequested(root.dockInstanceId)
            }

            Row {
                id: launchingFace
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.md
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xxs
                    width: 220

                    Text {
                        width: parent.width
                        text: root.dockLaunchProgress >= 0
                              ? qsTr("%1 · %2%").arg(root.dockLaunchStatus).arg(Math.round(root.dockLaunchProgress * 100))
                              : root.dockLaunchStatus
                        elide: Text.ElideRight
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        font.weight: Font.Medium
                    }
                    LaunchProgressBar {
                        width: parent.width
                        implicitHeight: 8
                        onMedia: true
                        segmented: true
                        progress: root.dockLaunchProgress
                    }
                }

                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.heightLg
                    flat: false
                    iconName: "x"
                    tip: qsTr("Cancel")
                    onClicked: root.cancelRequested(root.dockInstanceId)
                }
            }

            Row {
                id: runningFace
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.md
                opacity: !root.launching && root.dockRunning ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.sm

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 9
                        height: 9
                        radius: 4.5
                        color: Theme.palette.success

                        SequentialAnimation on opacity {
                            running: root.dockRunning && root.motionEnabled
                            loops: Animation.Infinite
                            NumberAnimation { from: 1.0; to: 0.4; duration: 900; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.4; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Playing")
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.bodyStrong.pixelSize
                        font.weight: Font.Bold
                    }
                }

                PlayButton {
                    anchors.verticalCenter: parent.verticalCenter
                    hero: true
                    round: false
                    size: Theme.control.heightLg + 6
                    running: true
                    onClicked: root.stopRequested(root.dockInstanceId)
                }
            }
        }
    }

    // -- The instance picker -------------------------------------------------
    Popup {
        id: picker
        x: Theme.space.xl + Theme.space.xs
        y: -height - Theme.space.sm
        width: 360
        // The list's own height already caps at listMaxHeight below; deriving
        // this from that exact same expression (rather than a separate
        // guessed constant) is what keeps the two from ever disagreeing --
        // a popup capped shorter than the list it contains was exactly why
        // the last row used to be sliced off mid-row. Tall enough for a
        // realistic recent-instances list (7-8 rows at this row height)
        // to show in full without scrolling at all; a library with more
        // than that scrolls, and lands on a whole row at the bottom via
        // the list's own matching topMargin/bottomMargin below.
        readonly property int listMaxHeight: 360
        // Search field + the ColumnLayout's own spacing + this popup's own
        // top/bottom padding -- everything in this popup that is not the
        // list itself.
        readonly property int chromeHeight: Theme.control.height + Theme.space.sm + padding * 2
        height: chromeHeight + Math.min(listMaxHeight, Math.max(1, list.contentHeight))
        padding: Theme.space.sm
        // Belt and braces alongside the height match above: nothing paints
        // past this popup's own rounded background.
        clip: true

        property string savedFilter: ""
        onOpened: {
            picker.savedFilter = root.instanceModel ? root.instanceModel.filterText : ""
            searchField.text = ""
            searchField.forceActiveFocus()
        }
        onClosed: if (root.instanceModel) root.instanceModel.filterText = picker.savedFilter

        contentItem: ColumnLayout {
            spacing: Theme.space.sm

            SearchBox {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search instances")
                onTextChanged: if (root.instanceModel) root.instanceModel.filterText = text
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(picker.listMaxHeight, Math.max(1, contentHeight))
                // Equal top and bottom breathing room, so a fully scrolled
                // list ends with clear space under the last row instead of
                // stopping exactly on its bottom edge.
                topMargin: Theme.space.xs
                bottomMargin: Theme.space.xs
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                // The default listing is most-recent-first; typing a search
                // switches to the full library so an instance that has
                // never been played can still be found.
                model: searchField.text.length > 0 ? root.instanceModel : root.recentModel
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: row
                    required property string instanceId
                    required property string name
                    required property string iconKey
                    required property bool isRunning
                    required property string gameVersion
                    required property string loader
                    width: list.width
                    hoverEnabled: true
                    highlighted: row.instanceId === root.selectedId

                    onClicked: {
                        root.selectRequested(row.instanceId)
                        picker.close()
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.sm

                        Image {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            source: row.iconKey.length > 0 ? "image://instanceicon/" + row.iconKey : ""
                            // A fixed size, not Qt.size(width, height): this
                            // Image's own width/height come from Layout.
                            // preferred* rather than a literal, and tying
                            // sourceSize back to them is exactly the binding
                            // loop QQuickImage warns about.
                            sourceSize: Qt.size(56, 56)
                            fillMode: Image.PreserveAspectFit
                        }
                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 0

                            Text {
                                width: parent.width
                                text: row.name
                                elide: Text.ElideRight
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                font.weight: Font.Medium
                            }
                            Text {
                                width: parent.width
                                text: Format.versionLine(row.loader, row.gameVersion)
                                elide: Text.ElideRight
                                color: Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.caption.pixelSize
                            }
                        }
                        Rectangle {
                            visible: row.isRunning
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: Theme.palette.success
                        }
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    title: qsTr("No instances found")
                    body: ""
                    actionText: ""

                    MeshIcon { iconName: "search"; size: 32; color: Theme.palette.textTertiary }
                }
            }
        }
    }
}
