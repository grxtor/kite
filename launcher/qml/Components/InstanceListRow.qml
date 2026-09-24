// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One instance in the library's list view: a dense row instead of
 * InstanceCard's cover art, for a library that would rather scan names than
 * browse thumbnails. Same roles and signals as InstanceCard (it is bound to
 * the same per-group model), so the two delegates are interchangeable from
 * InstanceSection's point of view -- only the layout differs.
 *
 * The trailing columns drop out at narrow widths rather than truncating into
 * nonsense; the name always keeps the room it needs to stay readable.
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

    property bool selected: false
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal stopRequested()
    signal menuRequested()

    readonly property bool hovered: forceHovered || hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    readonly property int iconExtent: Theme.control.height
    readonly property bool showVersionColumn: width > 460
    readonly property bool showStatsColumns: width > 620

    implicitWidth: 400
    implicitHeight: iconExtent + Theme.space.sm * 2

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested()

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: surface
        anchors.fill: parent
        radius: Theme.radius.md
        color: root.selected ? Theme.palette.accentSubtle
             : root.hovered ? Theme.palette.surfaceRaised : "transparent"
        border.width: root.selected ? 1 : 0
        border.color: Theme.palette.accent

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
            onDoubleTapped: root.playRequested()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked(); root.menuRequested() }
        }

        // The accent bar a selected InstanceCard shows as a border reads as
        // a stripe here instead -- a full card border on a thin row would
        // squeeze the content next to it.
        Rectangle {
            visible: root.selected
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
            width: 3
            radius: Theme.radius.xs
            color: Theme.palette.accent
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.sm
            spacing: Theme.space.md

            Item {
                Layout.preferredWidth: root.iconExtent
                Layout.preferredHeight: root.iconExtent

                Image {
                    id: icon
                    anchors.fill: parent
                    source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                }

                // Pulsing while running, same treatment as the sidebar's
                // Recent list -- one running-instance language everywhere.
                Rectangle {
                    visible: root.isRunning
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: -1
                    width: 9
                    height: 9
                    radius: width / 2
                    color: Theme.palette.success
                    border.width: 2
                    border.color: Theme.palette.canvas

                    SequentialAnimation on opacity {
                        running: root.isRunning
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 0.45; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }

            Text {
                visible: root.showVersionColumn
                Layout.preferredWidth: 150
                Layout.minimumWidth: 0
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.accent : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Font.Medium
            }

            Text {
                visible: root.showStatsColumns
                Layout.preferredWidth: 92
                horizontalAlignment: Text.AlignRight
                text: Format.lastPlayed(root.lastLaunch)
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            Text {
                visible: root.showStatsColumns
                Layout.preferredWidth: 72
                horizontalAlignment: Text.AlignRight
                text: Format.playTime(root.totalTimePlayed)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }

            Item {
                Layout.preferredWidth: Theme.control.heightSm * 2 + Theme.space.xs
                Layout.preferredHeight: root.iconExtent

                IconButton {
                    anchors.right: playButton.left
                    anchors.rightMargin: Theme.space.xs
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.heightSm
                    iconName: "more"
                    tip: qsTr("More")
                    opacity: root.hovered ? 1 : 0
                    visible: opacity > 0
                    focusPolicy: Qt.NoFocus
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    onClicked: root.menuRequested()
                }

                PlayButton {
                    id: playButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    size: Theme.control.heightSm + 4
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

    Rectangle {
        x: surface.x - 2
        y: surface.y - 2
        width: surface.width + 4
        height: surface.height + 4
        radius: surface.radius + 2
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
