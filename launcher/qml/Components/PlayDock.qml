// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The persistent bottom play bar: the selected instance on the left with a
 * picker to change it, the big Play button anchored at the right so it sits
 * in the window's bottom-right corner. Replaces the library's old
 * "Continue playing" hero -- one always-there place to press Play instead
 * of one that only showed up on the Library page.
 *
 * Part of the shell's frame (Main.qml), under the page panel rather than
 * floating over it: the page area shrinks by exactly this bar's height
 * whenever it is visible, so it can never cover a page's content. The
 * shell paints the surface (the brief's glass, shared with the sidebar and
 * the top bar) and the panel's rounded edge above is the divider, so the
 * bar draws no background of its own.
 *
 * `rowModel` is the shell's heroModel, freed up by removing the library's
 * own hero card; Main.qml points its instanceId at whichever instance this
 * bar should show (see its own dockInstanceId).
 *
 * Play is the one dominant control: 48 px tall, the widest thing in the
 * bar, the primary fill. Beside it, a separate ▾ opens the other ways to play the same
 * instance (LaunchOptionsMenu: offline, demo, as another account, a
 * profiler) -- Lunar's launch button and its version arrow; its choices
 * leave here as signals, like Play itself.
 *
 * No picture behind the controls: the pages above already carry the
 * artwork, and a picture under the bar's own text and Play button only made
 * both harder to read.
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
    // The picker's "Create instance", with no instance to pick yet.
    signal createRequested()

    // The Play options menu (LaunchOptionsMenu.qml): what it offers comes
    // from launchOptionsFor(id) -- Main.qml's launchOptions() -- and its
    // choices leave as the signals below.
    property var launchOptionsFor: null
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    // Every decorative loop stops when the window is not the active one and
    // when the user has asked for less motion.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive
                                          && !SettingsStore.bool("UiReduceMotion")

    implicitHeight: Theme.layout.barHeight

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

    // Opens the Play options menu above its chevron, right-aligned with it
    // (the bar sits at the bottom of the window, so there is no room
    // below). Also a dev-route step ("launchmenu", "launchmenu=playAs"),
    // which can open one of its submenus too.
    function openLaunchMenu(submenu) {
        if (!root.hasInstance)
            return
        launchMenu.refresh()
        launchMenu.popup(moreButton, moreButton.width - launchMenu.implicitWidth,
                         -launchMenu.implicitHeight - Theme.space.xs)
        if (submenu)
            Qt.callLater(() => launchMenu.openSubmenu(submenu))
    }

    // -- The current row, read out of rowModel's one row --------------------
    // A role model with 0-1 rows has no properties of its own to bind
    // to directly, so a throwaway delegate reads its roles into plain
    // properties everything else here can use.
    property string dockInstanceId: ""
    property string dockName: ""
    property string dockIconKey: ""
    // The colour the bar is drawn on (Main.qml's shell), for the corners of
    // an instance icon that fills its tile.
    property color surround: "transparent"
    property bool dockRunning: false
    property bool dockCanLaunch: false
    property var dockTotalTimePlayed: 0
    property string dockGameVersion: ""
    property string dockLoader: ""
    property string dockLaunchStatus: ""
    property real dockLaunchProgress: -1

    // A launch shown without one running, for a dev-route snapshot of the
    // launching state: { status: "Downloading assets", progress: 0.42 }
    // (progress -1 for "no idea how far"). Null for the real thing.
    property var previewLaunch: null

    // The launch as shown: the instance's own, or the preview.
    readonly property string launchStatus: root.previewLaunch ? String(root.previewLaunch.status || "")
                                                              : root.dockLaunchStatus
    readonly property real launchProgress: root.previewLaunch && root.previewLaunch.progress !== undefined
                                           ? root.previewLaunch.progress : root.dockLaunchProgress
    // What the launch is doing, as a label ("Downloading assets"): the
    // stage the launch task names (LaunchTask, MinecraftUpdate), already
    // without a trailing "..." (LaunchProgressTracker).
    readonly property string launchStage: root.launchStatus

    readonly property bool hasInstance: root.dockInstanceId.length > 0
    readonly property bool launching: root.launchStatus.length > 0

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
            onLaunchStatusChanged: sync()
            onLaunchProgressChanged: sync()
        }
    }
    // No row at all (library empty, or the model has not been pointed at an
    // id yet): fall back to blank rather than stale leftovers.
    onRowModelChanged: if (!rowModel) { dockInstanceId = ""; dockName = "" }

    // Play and its neighbours: the large control height the brief asks of
    // the main action, and a width that makes it the bar's anchor rather
    // than one more button.
    readonly property int playHeight: Theme.control.heightLg
    readonly property int playWidth: 176

    // The same column the page above uses (TopBar's rule), so Play's right
    // edge is the content's right edge and the bar centres with the page on
    // a wide window.
    readonly property int sideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                               Math.round((root.width - Theme.layout.maxContentWidth) / 2))

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        spacing: Theme.space.md

        // The instance this bar plays, as a dropdown button: icon, name and
        // a chevron, inset the same on every side. Quiet like the account
        // button (ProfileButton): no plate at rest, the shared hover tint on
        // hover and while its list is open, the chevron turning over.
        AbstractButton {
            id: pickerButton
            Layout.alignment: Qt.AlignVCenter
            // The icon tile's size; with the padding, the plate's height.
            readonly property int tileSize: 44
            padding: Theme.space.sm
            implicitWidth: implicitContentWidth + leftPadding + rightPadding
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            hoverEnabled: true
            Accessible.name: root.hasInstance ? qsTr("Change instance: %1").arg(root.dockName) : qsTr("Choose an instance")

            onClicked: picker.visible ? picker.close() : picker.open()

            background: Rectangle {
                radius: Theme.radius.md
                color: pickerButton.down ? Theme.palette.pressedOverlay
                     : pickerButton.hovered || picker.visible ? Theme.palette.hoverOverlay : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }

            contentItem: RowLayout {
                id: pickerRow
                spacing: Theme.space.sm

                // A neutral tile, the icon filling about 80 % of it -- the
                // same tile every instance icon sits on (designs/
                // phase3-scale.md, decision 3), never one tinted from it.
                // An instance's, so in the kite's cut, raised on hover.
                KiteShape {
                    Layout.preferredWidth: pickerButton.tileSize
                    Layout.preferredHeight: pickerButton.tileSize
                    radius: Theme.radius.md
                    color: Theme.palette.surfaceRaised
                    borderWidth: 1
                    borderColor: Theme.palette.border
                    lifted: pickerButton.hovered
                    readonly property bool iconFills: root.hasInstance && Icons.fillsTile(root.dockIconKey)
                    // An opaque square icon fills the tile, rounded in the
                    // colour around it: the bar under the button's tint.
                    matte: !iconFills ? "transparent"
                         : Qt.tint(root.surround, pickerButton.down ? Theme.palette.pressedOverlay
                                   : pickerButton.hovered || picker.visible ? Theme.palette.hoverOverlay : "transparent")

                    Image {
                        anchors.centerIn: parent
                        width: parent.iconFills ? parent.width : Math.round(pickerButton.tileSize * 0.8)
                        height: width
                        visible: root.hasInstance
                        source: root.dockIconKey.length > 0 ? Icons.instanceIcon(root.dockIconKey) : ""
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectFit
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: !root.hasInstance
                        iconName: "library"
                        size: Theme.icon.md
                        color: Theme.palette.textSecondary
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
                        Layout.maximumWidth: 240
                        text: root.hasInstance ? root.dockName : qsTr("Choose an instance")
                        elide: Text.ElideRight
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.bodyStrong.pixelSize
                        font.weight: Theme.type.bodyStrong.weight
                    }
                    Text {
                        id: subtitle
                        Layout.maximumWidth: 240
                        visible: root.hasInstance && subtitle.text.length > 0
                        readonly property string played: Format.playTime(root.dockTotalTimePlayed)
                        text: [Format.versionLine(root.dockLoader, root.dockGameVersion),
                               played.length > 0 ? qsTr("%1 played").arg(played) : ""]
                              .filter(s => s.length > 0).join("  ·  ")
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }

                MeshIcon {
                    Layout.alignment: Qt.AlignVCenter
                    iconName: "chevron-down"
                    size: Theme.icon.sm
                    color: Theme.palette.textSecondary
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
            Layout.preferredHeight: root.playHeight

            // Play, and beside it the ▾ for the other ways to play: a
            // secondary (bordered) button of the same height, so Play stays
            // the one primary action on screen.
            Row {
                id: readyFace
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.sm
                opacity: root.hasInstance && !root.launching && !root.dockRunning ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

                PlayButton {
                    anchors.verticalCenter: parent.verticalCenter
                    hero: true
                    round: false
                    size: root.playHeight
                    implicitWidth: root.playWidth
                    enabled: root.hasInstance && root.dockCanLaunch
                    onClicked: root.launchRequested(root.dockInstanceId)
                }
                IconButton {
                    id: moreButton
                    anchors.verticalCenter: parent.verticalCenter
                    size: root.playHeight
                    flat: false
                    iconName: "chevron-down"
                    icon.width: Theme.icon.md
                    icon.height: Theme.icon.md
                    tip: qsTr("More ways to play")
                    enabled: root.hasInstance && root.dockCanLaunch
                    onClicked: launchMenu.visible ? launchMenu.close() : root.openLaunchMenu()
                }
            }

            // Launching, where Play was: what the launch is doing right
            // now ("Downloading assets") and how far along it is, over a
            // bar in the accent colour, and Cancel beside it -- the way a
            // launcher's play button turns into a progress readout.
            RowLayout {
                id: launchingFace
                readonly property int readoutWidth: 280
                readonly property int barThickness: 6
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.lg
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

                Accessible.role: Accessible.ProgressBar
                Accessible.name: root.launchProgress >= 0
                                 ? qsTr("%1, %2 percent").arg(root.launchStage).arg(Math.round(root.launchProgress * 100))
                                 : root.launchStage

                ColumnLayout {
                    Layout.preferredWidth: launchingFace.readoutWidth
                    Layout.alignment: Qt.AlignVCenter
                    spacing: Theme.space.sm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.sm

                        Text {
                            Layout.fillWidth: true
                            text: root.launchStage
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.bodyStrong.pixelSize
                            font.weight: Theme.type.bodyStrong.weight
                        }
                        // Only once the step says how far it is.
                        Text {
                            visible: root.launchProgress >= 0
                            text: qsTr("%1%").arg(Math.round(root.launchProgress * 100))
                            color: Theme.palette.textSecondary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.label.pixelSize
                            font.weight: Theme.type.label.weight
                            // Digits of one width, so the number does not
                            // jitter as it counts up.
                            font.features: ({ "tnum": 1 })
                        }
                    }
                    LaunchProgressBar {
                        Layout.fillWidth: true
                        implicitHeight: launchingFace.barThickness
                        progress: root.launchProgress
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignVCenter
                    buttonSize: "lg"
                    text: qsTr("Cancel")
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
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.bodyStrong.pixelSize
                        font.weight: Font.Bold
                    }
                }

                PlayButton {
                    anchors.verticalCenter: parent.verticalCenter
                    hero: true
                    round: false
                    size: root.playHeight
                    implicitWidth: root.playWidth
                    running: true
                    onClicked: root.stopRequested(root.dockInstanceId)
                }
            }
        }
    }

    LaunchOptionsMenu {
        id: launchMenu
        instanceId: root.dockInstanceId
        optionsFor: root.launchOptionsFor
        onLaunchModeRequested: (id, mode) => root.launchModeRequested(id, mode)
        onLaunchAsRequested: (id, accountId) => root.launchAsRequested(id, accountId)
        onProfilerChosen: (id, key) => root.profilerChosen(id, key)
        onPerformanceChosen: (id, enabled) => root.performanceChosen(id, enabled)
        onNavigateRequested: (target) => root.navigateRequested(target)
    }

    // -- The instance picker -------------------------------------------------
    Popup {
        id: picker
        // Over the button that opens it, left edges aligned, with a clear
        // gap above the bar.
        x: root.sideMargin
        y: -height - Theme.space.md
        width: 360
        // Every row is this tall: two lines (name, version) and the
        // delegate's padding. Fixed, so the list can be sized in whole rows.
        readonly property int rowHeight: Theme.control.heightLg + Theme.space.xs
        // Seven rows at most, whole ones: a longer list scrolls, and its
        // bottom edge fades (ScrollFade) rather than slicing a row.
        readonly property int visibleRows: 7
        readonly property int listMaxHeight: rowHeight * visibleRows + Theme.space.xs * 2
        // Search field + the ColumnLayout's own spacing + this popup's own
        // top/bottom padding -- everything in this popup that is not the
        // list itself.
        // Nothing in the library at all (not just no search match): no
        // search field then, only the way to make the first instance.
        readonly property int recentCount: root.recentModel && root.recentModel.count !== undefined
                                           ? root.recentModel.count : 0
        readonly property bool libraryEmpty: searchField.text.length === 0 && picker.recentCount === 0
                                             && !!root.instanceModel && root.instanceModel.count === 0
        // Search field + the ColumnLayout's own spacing + this popup's own
        // top/bottom padding -- everything in this popup that is not the
        // list itself.
        readonly property int chromeHeight: (picker.libraryEmpty ? 0 : Theme.control.height + Theme.space.sm)
                                            + padding * 2
        // The list's own height; with no rows, the empty state's.
        readonly property int listAreaHeight: list.count === 0
            ? emptyState.implicitHeight + Theme.space.xl * 2
            : Math.min(listMaxHeight, list.contentHeight + list.topMargin + list.bottomMargin)
        height: chromeHeight + listAreaHeight
        padding: Theme.space.sm
        // Belt and braces alongside the height match above: nothing paints
        // past this popup's own rounded background.
        clip: true

        property string savedFilter: ""
        onOpened: {
            picker.savedFilter = root.instanceModel ? root.instanceModel.filterText : ""
            searchField.text = ""
            // The library whole, whatever a page's own search left on it.
            if (root.instanceModel) root.instanceModel.filterText = ""
            // Ready to type into, but with the plain focused border, not the
            // keyboard focus ring: nobody tabbed here. (The ring is for
            // keyboard focus; see Style/TextField.qml.)
            searchField.forceActiveFocus(Qt.MouseFocusReason)
        }
        onClosed: if (root.instanceModel) root.instanceModel.filterText = picker.savedFilter

        contentItem: ColumnLayout {
            spacing: Theme.space.sm

            SearchBox {
                id: searchField
                visible: !picker.libraryEmpty
                Layout.fillWidth: true
                placeholderText: qsTr("Search instances")
                onTextChanged: if (root.instanceModel) root.instanceModel.filterText = text
            }

            // The list, with a fade over its bottom edge while more rows
            // wait below it.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: picker.listAreaHeight

                ListView {
                    id: list
                    anchors.fill: parent
                    // Equal top and bottom breathing room, so a fully scrolled
                    // list ends with clear space under the last row instead of
                    // stopping exactly on its bottom edge.
                    topMargin: Theme.space.xs
                    bottomMargin: Theme.space.xs
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    // The default listing is most-recent-first; typing a search
                    // switches to the full library so an instance that has
                    // never been played can still be found. So does a library
                    // nothing has been played from yet.
                    model: searchField.text.length > 0 || picker.recentCount === 0
                           ? root.instanceModel : root.recentModel
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
                        height: picker.rowHeight
                        hoverEnabled: true
                        // The instance the bar plays right now -- which is the
                        // selection when there is one, otherwise the most recent
                        // -- so the list always shows where "Play" points.
                        readonly property bool current: row.instanceId === root.dockInstanceId
                        highlighted: row.current

                        onClicked: {
                            root.selectRequested(row.instanceId)
                            picker.close()
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space.sm

                            Image {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                source: row.iconKey.length > 0 ? Icons.instanceIcon(row.iconKey) : ""
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
                            MeshIcon {
                                visible: row.current
                                iconName: "check"
                                size: Theme.icon.sm
                                color: Theme.palette.accentText
                            }
                        }
                    }
                }

                EmptyState {
                    id: emptyState
                    anchors.centerIn: parent
                    visible: list.count === 0
                    title: picker.libraryEmpty ? qsTr("No instances yet") : qsTr("No matches")
                    body: picker.libraryEmpty ? qsTr("Create one to start playing.")
                                              : qsTr("Try a different name.")
                    actionText: picker.libraryEmpty ? qsTr("Create instance") : ""
                    actionIcon: "plus"
                    onActionTriggered: {
                        picker.close()
                        root.createRequested()
                    }

                    MeshIcon {
                        iconName: picker.libraryEmpty ? "cube" : "search"
                        size: Theme.icon.lg + Theme.space.md
                        color: Theme.palette.textTertiary
                    }
                }

                ScrollFade {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    flickable: list
                    fadeTo: Theme.palette.surfaceOverlay
                }
            }
        }
    }
}
