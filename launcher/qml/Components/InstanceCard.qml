// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One instance in the library grid: a cover tinted from the instance's own
 * icon, the name, and what it runs. Used directly as a delegate -- the
 * required properties are filled from InstanceList's named roles, so this
 * file never mentions role numbers.
 *
 * Play lives on the cover and appears on hover (always while running, as
 * Stop); click selects, double-click plays, right-click or the "more"
 * button asks the page for the instance menu.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property string gameVersion
    required property string loader
    required property color iconTint
    // Newest screenshot url (InstanceList's coverImage role), or "" when
    // the instance has none yet -- the cover's own art, see CoverArt.qml.
    required property string coverImage
    // Empty while nothing is being launched; progress is 0..1, or negative
    // while the launch cannot say how far along it is.
    required property string launchStatus
    required property real launchProgress

    property bool selected: false
    // Lets a static review page (Gallery.qml) show the hover state.
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal stopRequested()
    signal menuRequested()

    readonly property bool hovered: forceHovered || hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    readonly property int inset: Theme.space.sm - 2
    readonly property int coverHeight: Math.round((width - inset * 2) * 0.6)
    readonly property int coverRadius: Theme.radius.md + 2

    implicitWidth: 208
    implicitHeight: inset + coverHeight + Theme.space.md + Theme.type.bodyStrong.lineHeightPx
                    + Theme.space.xxs + Theme.type.caption.lineHeightPx + Theme.space.md

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested()

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: card
        width: parent.width
        height: parent.height
        radius: Theme.radius.lg
        color: root.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : root.hovered ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // ReleaseWithinBounds grabs on press, so the page's "click empty
        // space to deselect" handler underneath never sees a card click.
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

        Item {
            id: cover
            x: root.inset
            y: root.inset
            width: parent.width - root.inset * 2
            height: root.coverHeight

            CoverArt {
                id: art
                anchors.fill: parent
                radius: root.coverRadius
                source: root.coverImage
                tint: root.iconTint
                iconKey: root.iconKey
                seed: root.instanceId
                iconSize: Math.max(48, Math.min(96, Math.round(root.coverHeight * 0.55)))
                // Protects the running pill/more button up top and the
                // play button/icon badge down below from a bright photo.
                scrim: "bottom"
                hovered: root.hovered
                matte: card.color
            }

            // While launching the cover dims and a bar runs along its foot.
            Rectangle {
                anchors.fill: parent
                radius: root.coverRadius
                color: Qt.rgba(0, 0, 0, 0.45)
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
            }

            LaunchProgressBar {
                visible: root.launching
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space.sm
                onMedia: true
                progress: root.launchProgress
            }

            // Running: a pill in the corner, readable on any tint.
            Rectangle {
                visible: root.isRunning
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.space.sm
                radius: height / 2
                height: Theme.control.heightSm - 6
                width: runningRow.implicitWidth + Theme.space.sm * 2
                color: Qt.rgba(0, 0, 0, 0.55)

                Row {
                    id: runningRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs + 1
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        // A perfect circle, like Switch/Slider's own round
                        // parts: computed half-width, not a radius token.
                        width: 7; height: 7; radius: width / 2
                        color: Theme.palette.success
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Running")
                        color: "white"
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize - 1
                        font.weight: Font.DemiBold
                    }
                }
            }

            IconButton {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space.xs
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
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space.sm
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

        // A small badge for the instance icon, peeking over the cover's
        // bottom-left edge -- only shown once there is a real screenshot
        // to badge; the fallback cover already *is* the icon, large and
        // centred, and a second copy of it would just be clutter.
        Item {
            id: iconBadge
            readonly property int size: 36
            visible: art.hasPhoto
            x: cover.x + Theme.space.sm
            y: cover.y + cover.height - size * 0.8
            width: size
            height: size

            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                radius: Theme.radius.md + 2
                color: Qt.rgba(0, 0, 0, Theme.dark ? 0.4 : 0.18)
            }

            Rectangle {
                anchors.fill: parent
                radius: Theme.radius.md
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                Image {
                    anchors.fill: parent
                    anchors.margins: 5
                    source: root.iconKey.length > 0 ? "image://instanceicon/" + root.iconKey : ""
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                }
            }
        }

        Text {
            id: title
            anchors.top: cover.bottom
            anchors.topMargin: Theme.space.md - 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            text: root.name
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.bodyStrong.pixelSize
            font.weight: Theme.type.bodyStrong.weight
        }

        Item {
            anchors.top: title.bottom
            anchors.topMargin: Theme.space.xxs
            anchors.left: title.left
            anchors.right: title.right
            height: Theme.type.caption.lineHeightPx

            Text {
                id: versionText
                anchors.left: parent.left
                anchors.right: timeText.left
                anchors.rightMargin: Theme.space.sm
                anchors.verticalCenter: parent.verticalCenter
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.accent : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Font.Medium
            }

            Text {
                id: timeText
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.launching ? (root.launchProgress >= 0 ? Math.round(root.launchProgress * 100) + "%" : "")
                                     : Format.lastPlayed(root.lastLaunch)
                color: root.launching ? Theme.palette.accent : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // selected border.
        x: card.x - 3
        y: card.y - 3
        width: card.width + 6
        height: card.height + 6
        radius: card.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
