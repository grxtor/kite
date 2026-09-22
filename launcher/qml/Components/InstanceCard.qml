// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One tile in InstanceGrid. Used directly as a GridView delegate: the
 * required properties below are populated from InstanceList's named roles by
 * Qt's automatic role-to-required-property matching, so this file never
 * mentions role indices or a model at all.
 *
 * totalTimePlayed is a role InstanceList exposes but this card does not
 * render, so it is intentionally not declared here -- the brief only asks
 * for the roles actually used.
 */
Rectangle {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property string group
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch

    // Not a model role: InstanceGrid sets this by comparing instanceId
    // against its own selectedId.
    property bool selected: false

    // Demo-only escape hatch: hover is normally driven purely by the mouse,
    // which a static review page (Gallery.qml) can never trigger. Letting a
    // caller force it lets the gallery show what hover looks like without
    // faking pointer events.
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal contextMenuRequested(point pos)

    // There is no dedicated "instance thumbnail" size in the token contract
    // (Theme.icon tops out at lg = 24, sized for inline UI glyphs, not a
    // grid tile's headline icon), so this is derived from it rather than
    // written as a bare number.
    readonly property int iconExtent: Theme.icon.lg * 3

    readonly property bool hovered: root.forceHovered || mouseArea.containsMouse

    radius: Theme.radius.lg
    color: root.selected ? Theme.palette.accentSubtle
                          : root.hovered ? Theme.palette.surfaceRaised
                                         : Theme.palette.surface
    border.width: root.selected || root.hovered ? 1 : 0
    border.color: root.selected ? Theme.palette.accent : Theme.palette.border

    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

    // Keyboard-navigable: the focus ring below is meaningless if the card
    // can never actually receive focus via Tab.
    activeFocusOnTab: true
    Keys.onReturnPressed: root.clicked()

    // Content is centred rather than pinned to a computed height, so small
    // drift between this file's natural size and InstanceGrid's cellHeight
    // formula reads as harmless padding instead of a layout glitch.
    Column {
        anchors.centerIn: parent
        width: parent.width - Theme.space.lg * 2
        spacing: Theme.space.sm

        Rectangle {
            id: iconFrame
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.iconExtent
            height: root.iconExtent
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken
            clip: true

            Image {
                anchors.fill: parent
                source: "image://instanceicon/" + root.iconKey
                // Requested in DIPs, at the size the icon is actually drawn.
                sourceSize: Qt.size(iconFrame.width, iconFrame.height)
                fillMode: Image.PreserveAspectFit
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.name
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.bodyStrong.pixelSize
            font.weight: Theme.type.bodyStrong.weight
            lineHeight: Theme.type.bodyStrong.lineHeight
            // Theme line heights are multipliers. Text.FixedHeight would read
            // 1.45 as pixels and stack a wrapped second line on the first.
            lineHeightMode: Text.ProportionalHeight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.secondaryLine
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
            font.weight: Theme.type.caption.weight
            elide: Text.ElideRight
        }
    }

    // Declared before the running badge, Play button and focus ring below so
    // those overlays stay on top of it in both paint and hit-test order --
    // otherwise this full-size MouseArea would swallow the Play button's
    // clicks before they ever reached it.
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => {
            root.forceActiveFocus()
            if (mouse.button === Qt.RightButton)
                root.contextMenuRequested(Qt.point(mouse.x, mouse.y))
            else
                root.clicked()
        }
        onDoubleClicked: (mouse) => {
            if (mouse.button === Qt.LeftButton)
                root.doubleClicked()
        }
    }

    // Running state is a corner badge rather than another line in the
    // Column above, so a running instance's card is not taller than every
    // other card in the same row.
    Rectangle {
        id: runningBadge
        visible: root.isRunning
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.space.xs
        radius: Theme.radius.pill
        color: Theme.palette.successSubtle
        implicitWidth: runningRow.implicitWidth + Theme.space.sm * 2
        implicitHeight: runningRow.implicitHeight + Theme.space.xxs * 2

        Row {
            id: runningRow
            anchors.centerIn: parent
            spacing: Theme.space.xxs

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.space.xs
                height: Theme.space.xs
                radius: width / 2
                color: Theme.palette.success
            }

            Text {
                text: qsTr("Running")
                color: Theme.palette.success
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Theme.type.caption.weight
            }
        }
    }

    // Standard Controls Button, unstyled here on purpose: a separate style
    // component skins every Button project-wide, so hand-rolling chrome for
    // this one would fight it. Only visible on hover/selection so the grid
    // reads as icons+names at rest, per the "modern launcher" brief.
    Button {
        text: qsTr("Play")
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space.sm
        visible: root.hovered || root.selected
        enabled: root.canLaunch
        onClicked: root.playRequested()
    }

    Rectangle {
        // Focus ring: drawn as an outset outline rather than a border on the
        // card itself, so it never competes with the selected/hover border.
        anchors.fill: parent
        anchors.margins: -3
        radius: parent.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }

    // "Today" / "N days ago" / "Never", per the brief. lastLaunch arrives as
    // whatever the source model gives InstanceList's date/number role, so
    // both a JS Date and a raw epoch number (seconds or milliseconds) are
    // accepted rather than assuming one representation.
    function lastLaunchDate() {
        if (root.lastLaunch === undefined || root.lastLaunch === null)
            return null
        var date = (root.lastLaunch instanceof Date)
                ? root.lastLaunch
                : new Date(Number(root.lastLaunch) > 1e12
                           ? Number(root.lastLaunch)
                           : Number(root.lastLaunch) * 1000)
        if (isNaN(date.getTime()) || date.getTime() <= 0)
            return null
        return date
    }

    function relativeLastLaunch() {
        var date = root.lastLaunchDate()
        if (date === null)
            return qsTr("Never")
        var now = new Date()
        var startOfToday = new Date(now.getFullYear(), now.getMonth(), now.getDate())
        var startOfThat = new Date(date.getFullYear(), date.getMonth(), date.getDate())
        var diffDays = Math.round((startOfToday - startOfThat) / 86400000)
        if (diffDays <= 0)
            return qsTr("Today")
        if (diffDays === 1)
            return qsTr("Yesterday")
        return qsTr("%1 days ago").arg(diffDays)
    }

    readonly property string secondaryLine: root.group.length > 0
            ? qsTr("%1 • %2").arg(root.group).arg(root.relativeLastLaunch())
            : root.relativeLastLaunch()
}
