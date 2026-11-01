// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/* The bottom play bar: selected instance, picker, Play and launch options. */
Item {
    id: root

    // Single-row model (QmlShell.heroModel) bound to the shown instance.
    property var rowModel: null
    // The full library list, for the picker's search field.
    property var instanceModel: null
    // Most-recently-played first, for the picker's default listing.
    property var recentModel: null
    property string selectedId: ""

    signal selectRequested(string id)
    signal launchRequested(string id)
    signal stopRequested(string id)
    signal cancelRequested(string id)
    // The picker's "Create instance", with no instance to pick yet.
    signal createRequested()

    // What the Play options menu offers; its choices leave as the signals below.
    property var launchOptionsFor: null
    signal launchModeRequested(string id, string mode)
    signal launchAsRequested(string id, string accountId)
    signal profilerChosen(string id, string profilerKey)
    signal performanceChosen(string id, bool enabled)
    signal navigateRequested(string target)

    // Decorative loops stop while the window is inactive.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive && !Theme.motion.reduced

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

    // Dev-route snapshots open the picker without a click.
    function openPicker() { picker.open() }

    // Opens the Play options menu above its chevron; also a dev-route step ("launchmenu",
    // "launchmenu=playAs").
    function openLaunchMenu(submenu) {
        if (!root.hasInstance)
            return
        launchMenu.refresh()
        launchMenu.popup(moreButton, moreButton.width - launchMenu.implicitWidth,
                         -launchMenu.implicitHeight - Theme.space.xs)
        // The closed menu's height is a guess; hold the bottom edge above the chevron once it has settled.
        launchMenu.y = Qt.binding(() => -launchMenu.height - Theme.space.xs)
        if (submenu)
            Qt.callLater(() => launchMenu.openSubmenu(submenu))
    }

    // A 0-1 row role model has no bindable properties, so a throwaway delegate reads its roles.
    property string dockInstanceId: ""
    property string dockName: ""
    property string dockIconKey: ""
    // Colour behind the bar, for the corners of a full-tile icon.
    property color surround: "transparent"
    property bool dockRunning: false
    property bool dockCanLaunch: false
    property var dockTotalTimePlayed: 0
    property string dockGameVersion: ""
    property string dockLoader: ""
    property string dockLaunchStatus: ""
    property real dockLaunchProgress: -1

    // A launch shown without one running, for dev-route snapshots: { status, progress }; progress
    // -1 = unknown.
    property var previewLaunch: null

    readonly property string launchStatus: root.previewLaunch ? String(root.previewLaunch.status || "")
                                                              : root.dockLaunchStatus
    readonly property real launchProgress: root.previewLaunch && root.previewLaunch.progress !== undefined
                                           ? root.previewLaunch.progress : root.dockLaunchProgress
    // Launch stage label ("Downloading assets"), without a trailing "...".
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
    // No row (empty library or model not pointed yet): blank, not stale.
    onRowModelChanged: if (!rowModel) { dockInstanceId = ""; dockName = "" }

    readonly property int playHeight: Theme.control.heightLg
    readonly property int playWidth: 176

    // The page column's margin (Main.pageSideMargin), so the bar lines up with the page above it.
    property int sideMargin: Theme.space.xl + Theme.space.xs

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        spacing: Theme.space.md

        AbstractButton {
            id: pickerButton
            Layout.alignment: Qt.AlignVCenter
            readonly property int tileSize: 44
            padding: Theme.space.sm
            implicitWidth: implicitContentWidth + leftPadding + rightPadding
            implicitHeight: implicitContentHeight + topPadding + bottomPadding
            hoverEnabled: true
            Accessible.name: root.hasInstance ? qsTr("Change instance: %1").arg(root.dockName) : qsTr("Choose an instance")

            onClicked: picker.visible ? picker.close() : picker.open()

            background: StateFill {
                radius: Theme.radius.md
                hovered: pickerButton.hovered || picker.visible
                down: pickerButton.down
            }

            contentItem: RowLayout {
                spacing: Theme.space.sm

                KiteShape {
                    Layout.preferredWidth: pickerButton.tileSize
                    Layout.preferredHeight: pickerButton.tileSize
                    radius: Theme.radius.md
                    color: "transparent"
                    lifted: pickerButton.hovered
                    readonly property string drawnKey: Icons.displayKey(root.dockIconKey, root.dockInstanceId)
                    readonly property bool iconFills: root.hasInstance && Icons.fillsTile(drawnKey)
                    // An opaque square icon fills the tile, rounded in the colour around it.
                    matte: !iconFills || !Theme.solidShell ? "transparent"
                         : Qt.tint(root.surround, pickerButton.down ? Theme.palette.pressedOverlay
                                   : pickerButton.hovered || picker.visible ? Theme.palette.hoverOverlay : "transparent")

                    Image {
                        anchors.centerIn: parent
                        width: parent.iconFills ? parent.width : Math.round(pickerButton.tileSize * 0.8)
                        height: width
                        visible: root.hasInstance
                        source: parent.drawnKey.length > 0 ? Icons.instanceIcon(parent.drawnKey) : ""
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

                // ColumnLayout, not a fixed-width Column: short names sit close to the chevron;
                // Layout.maximumWidth caps long ones.
                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1

                    Copy {
                        Layout.maximumWidth: 240
                        text: root.hasInstance ? root.dockName : qsTr("Choose an instance")
                        role: "bodyStrong"
                        elide: Text.ElideRight
                    }
                    Copy {
                        id: subtitle
                        Layout.maximumWidth: 240
                        visible: root.hasInstance && subtitle.text.length > 0
                        readonly property string played: Format.playTime(root.dockTotalTimePlayed)
                        text: [Format.versionLine(root.dockLoader, root.dockGameVersion),
                               played.length > 0 ? qsTr("%1 played").arg(played) : ""]
                              .filter(s => s.length > 0).join("  ·  ")
                        role: "caption"
                        tone: "secondary"
                        elide: Text.ElideRight
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

        // Ready / launching / running cross-fade into each other.
        Item {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Math.max(readyFace.implicitWidth, launchingFace.implicitWidth, runningFace.implicitWidth)
            Layout.preferredHeight: root.playHeight

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

                        Copy {
                            Layout.fillWidth: true
                            text: root.launchStage
                            role: "bodyStrong"
                            elide: Text.ElideRight
                        }
                        Copy {
                            visible: root.launchProgress >= 0
                            text: qsTr("%1%").arg(Math.round(root.launchProgress * 100))
                            role: "label"
                            tone: "secondary"
                            font.weight: Theme.type.label.weight
                            // Equal-width digits stop the number jittering.
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
                            NumberAnimation { from: 1.0; to: 0.4; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.4; to: 1.0; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
                        }
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Playing")
                        role: "bodyStrong"
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

    Popup {
        id: picker
        x: root.sideMargin
        y: -height - Theme.space.md
        width: 360
        // Fixed, so the list is sized in whole rows.
        readonly property int rowHeight: Theme.control.heightLg + Theme.space.xs
        readonly property int visibleRows: 7
        readonly property int listMaxHeight: rowHeight * visibleRows + Theme.space.xs * 2
        readonly property int recentCount: root.recentModel && root.recentModel.count !== undefined
                                           ? root.recentModel.count : 0
        // No instances at all (not just no search match): only "Create instance" is offered.
        readonly property bool libraryEmpty: searchField.text.length === 0 && picker.recentCount === 0
                                             && !!root.instanceModel && root.instanceModel.count === 0
        // Search field, spacing and padding: everything but the list.
        readonly property int chromeHeight: (picker.libraryEmpty ? 0 : Theme.control.height + Theme.space.sm)
                                            + padding * 2
        readonly property int listAreaHeight: list.count === 0
            ? emptyState.implicitHeight + Theme.space.xl * 2
            : Math.min(listMaxHeight, list.contentHeight + list.topMargin + list.bottomMargin)
        height: chromeHeight + listAreaHeight
        padding: Theme.space.sm
        clip: true

        property string savedFilter: ""
        onOpened: {
            picker.savedFilter = root.instanceModel ? root.instanceModel.filterText : ""
            searchField.text = ""
            if (root.instanceModel) root.instanceModel.filterText = ""
            // Plain focus border, not the keyboard ring: nobody tabbed here.
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

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: picker.listAreaHeight

                ListView {
                    id: list
                    anchors.fill: parent
                    topMargin: Theme.space.xs
                    bottomMargin: Theme.space.xs
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    // Typing a search, or an unplayed library, switches from the recent list to the
                    // full library.
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
                                source: row.iconKey.length > 0 ? Icons.instanceIcon(Icons.displayKey(row.iconKey, row.instanceId)) : ""
                                // Fixed size: tying sourceSize to the Image's Layout-driven
                                // width/height is a binding loop.
                                sourceSize: Qt.size(56, 56)
                                fillMode: Image.PreserveAspectFit
                            }
                            Column {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 0

                                Copy {
                                    width: parent.width
                                    text: row.name
                                    elide: Text.ElideRight
                                    font.weight: Font.Medium
                                }
                                Copy {
                                    width: parent.width
                                    text: Format.versionLine(row.loader, row.gameVersion)
                                    role: "caption"
                                    tone: "tertiary"
                                    elide: Text.ElideRight
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
                        size: Theme.icon.xl
                        color: Theme.palette.textTertiary
                    }
                }

                ScrollFade {
                    flickable: list
                    fadeTo: Theme.palette.surfaceOverlay
                }
            }
        }
    }
}
