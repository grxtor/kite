// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One instance in the library's list view: InstanceCard laid out as a row,
 * for a library that would rather scan names than browse icons. Same roles
 * and signals as InstanceCard (it is bound to the same per-group model), so
 * the two delegates are interchangeable from InstanceSection's point of
 * view -- and the same grammar: the icon on the calm neutral square
 * (or the pack's own logo across it), the same surface, hover lift,
 * selected ring, and Play fading in on hover.
 *
 * The trailing columns drop out at narrow widths rather than truncating into
 * nonsense; the name always keeps the room it needs to stay readable.
 *
 * While a launch runs it takes Play's place, as on the card and the play
 * bar: a bar where Play sits, the stage ("Downloading assets") in the
 * version column and how far it is in the last-played one, and the
 * running dot in the warning colour until the game is actually up.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property var totalTimePlayed
    required property string gameVersion
    required property string loader
    required property color iconTint
    required property string launchStatus
    required property real launchProgress
    // A platform pack's own logo (InstanceList's packArt role), shown across
    // the icon square as on the grid card; "" for every other instance.
    property string packArt: ""

    property bool selected: false
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click or the Menu key, which open at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: forceHovered || hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    // The icon square, and the share of it the icon fills -- the grid
    // card's own proportions, smaller: a 48px square.
    readonly property int squareSize: Theme.control.heightLg
    readonly property real iconShare: 0.84
    readonly property int iconSize: Math.round(squareSize * iconShare)
    readonly property int runningDotSize: 9
    readonly property string iconSource: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")
    readonly property int versionColumnWidth: 168
    readonly property int lastPlayedColumnWidth: 104
    readonly property int playTimeColumnWidth: 84
    readonly property bool showVersionColumn: width > 480
    readonly property bool showStatsColumns: width > 660

    implicitWidth: 400
    // About 72px, between Home's compact rows and its cards. The square's
    // inset is also what keeps the logo's corner matte inside the row.
    implicitHeight: squareSize + Theme.space.md * 2

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Theme.radius.lg
        // The grid card's hover: one step lighter and a small shadow.
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: surface.radius
            level: Theme.elevation.sm
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
            onDoubleTapped: root.playRequested()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked(); root.menuRequested(null) }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.lg

            // The instance's tile, raised while the row is hovered, as the
            // grid card's is.
            InstanceTile {
                Layout.preferredWidth: root.squareSize
                Layout.preferredHeight: root.squareSize
                radius: Theme.radius.md
                matte: root.showsPackArt ? surface.color : "transparent"
                lifted: root.hovered

                Image {
                    anchors.centerIn: parent
                    visible: !root.showsPackArt
                    width: root.iconSize
                    height: root.iconSize
                    source: root.iconSource
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    // Rendered artwork: filtered and mipmapped, so any
                    // scaling the scene graph does stays clean.
                    smooth: true
                    mipmap: true
                }
                Image {
                    id: packLogo
                    anchors.fill: parent
                    visible: root.showsPackArt
                    source: root.packArt
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }

                // Pulsing while running, same treatment as the sidebar's
                // Recent list -- one running-instance language everywhere.
                // Amber while the launch is still starting the game. Over
                // the logo's corner matte, which it overlaps.
                Rectangle {
                    z: 2
                    visible: root.isRunning || root.launching
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -Theme.space.xxs
                    width: root.runningDotSize
                    height: width
                    radius: width / 2
                    color: root.launching ? Theme.palette.warning : Theme.palette.success
                    border.width: 2
                    border.color: surface.color

                    SequentialAnimation on opacity {
                        running: root.isRunning || root.launching
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.45; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                    }
                }
            }

            Text {
                id: nameText
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Theme.type.title.weight

                HoverHandler { id: nameHover; enabled: nameText.truncated }
                ToolTip.visible: nameText.truncated && nameHover.hovered
                ToolTip.delay: 500
                ToolTip.text: root.name
            }

            Text {
                visible: root.showVersionColumn
                Layout.preferredWidth: root.versionColumnWidth
                Layout.minimumWidth: 0
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }

            Text {
                visible: root.showStatsColumns
                Layout.preferredWidth: root.lastPlayedColumnWidth
                horizontalAlignment: Text.AlignRight
                text: !root.launching ? Format.lastPlayed(root.lastLaunch)
                    : root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.textSecondary : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                // Digits of one width, so the number does not jitter.
                font.features: ({ "tnum": 1 })
            }

            Text {
                // "Show play time per instance" (Settings > Minecraft).
                visible: root.showStatsColumns && SettingsStore.bool("ShowGameTime")
                Layout.preferredWidth: root.playTimeColumnWidth
                horizontalAlignment: Text.AlignRight
                text: Format.playTime(root.totalTimePlayed)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.features: ({ "tnum": 1 })
            }

            Item {
                Layout.preferredWidth: Theme.control.height * 2 + Theme.space.xs
                Layout.preferredHeight: root.squareSize

                IconButton {
                    id: moreButton
                    anchors.right: playButton.left
                    anchors.rightMargin: Theme.space.xs
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.height
                    iconName: "more"
                    tip: qsTr("More")
                    opacity: root.hovered && !root.launching ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.menuRequested(moreButton)
                }

                // Where Play sits, across the whole slot.
                LaunchProgressBar {
                    visible: root.launching
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    progress: root.launchProgress
                }

                PlayButton {
                    id: playButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.height
                    running: root.isRunning
                    enabled: root.isRunning || root.canLaunch
                    // A plain fade, no overshoot scale-in (design-plan.md §4/§2.4).
                    opacity: !root.launching && (root.hovered || root.isRunning) ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
                }
            }
        }
    }

    // The hairline, over everything so the logo's corner matte never
    // paints across it. Selected: a 2px accent ring.
    Rectangle {
        anchors.fill: surface
        radius: surface.radius
        color: "transparent"
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        // Keyboard focus ring, outset like InstanceCard's.
        anchors.fill: surface
        anchors.margins: -Theme.space.xs
        radius: surface.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
