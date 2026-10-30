// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One recently played instance in the sidebar: its icon on a small neutral
 * tile (the instance's own picture, Modrinth's rail), the name beside it in
 * the labelled sidebar. A click opens the instance; in the labelled sidebar
 * a play button appears on hover and launches it straight away. On the
 * icon rail (`compact`) the name is a tooltip and there is no play button
 * -- there is no room for it, and a click already opens the instance.
 */
AbstractButton {
    id: control

    // Filled straight from the instance model's roles when used as a
    // delegate; declared here rather than by the caller, since a caller
    // redeclaring them would shadow these and leave this file reading blanks.
    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning

    property bool compact: false

    // Every decorative loop in the shell stops when the window is not the
    // active one and when the user has asked for less motion -- repeated
    // per file rather than centralised, since SettingsStore/Theme are not
    // this change's to extend.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive
                                          && !SettingsStore.bool("UiReduceMotion")

    signal playRequested()

    implicitHeight: Theme.control.height
    implicitWidth: 200
    hoverEnabled: true

    Accessible.name: name

    // Beside the rail, as NavItem's.
    ToolTip {
        visible: control.compact && control.hovered
        delay: 400
        text: control.name
        x: control.width + Theme.space.md
        y: Math.round((control.height - implicitHeight) / 2)
    }

    // The same square, axis and label column as the NavItems above it
    // (SidebarNav sets all three), so the rail reads as one column and
    // hover is the same shape on every item.
    property int railSquare: Theme.control.height
    property int cornerRadius: Theme.radius.md
    property int iconAxis: Theme.space.md + Theme.icon.md / 2
    property int labelX: control.iconAxis + control.tileSize / 2 + Theme.space.sm
    // The tile nearly fills the rail's square: an instance icon is a picture
    // and reads by its picture rather than by a line shape. The icon fills
    // most of it, with a small corner, so the kite's cut and the rounding
    // don't leave the tile looking empty around a small icon.
    readonly property int tileSize: Theme.control.height
    readonly property int pictureSize: Math.round(control.tileSize * 0.86)
    // An opaque square icon (Icons.fillsTile) fills the tile instead, its
    // corners rounded in `surround`: what the rail shows around the tile
    // (SidebarNav sets it), under this item's hover or press tint.
    readonly property bool iconFills: Icons.fillsTile(control.drawnKey)
    // Its own icon, or the built-in one it was given (Icons.displayKey).
    readonly property string drawnKey: Icons.displayKey(control.iconKey, control.instanceId)
    property color surround: "transparent"
    // The open instance. On the folded rail its tile lights the kite's
    // nose in its own cut, as a selected library card does, instead of
    // SidebarNav's selection square framing it.
    property bool selected: false

    background: Rectangle {
        width: control.compact ? control.railSquare : control.width
        height: control.compact ? control.railSquare : control.height
        // Centred on the icon (see `tile` below), not on the row.
        x: control.compact ? control.iconAxis - width / 2 : 0
        y: control.compact ? (control.height - height) / 2 : 0
        radius: control.cornerRadius
        // On the icon rail the tile itself answers the pointer (it lifts);
        // a square behind it made a box around a box.
        color: control.compact ? "transparent"
             : control.down ? Theme.palette.pressedOverlay
             : control.hovered ? Theme.palette.hoverOverlay : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        // An instance, so its tile wears the kite's cut and lifts while
        // hovered (KiteShape).
        KiteShape {
            id: tile
            // At the same x in both modes, never re-anchored: the icon rail
            // is exactly as wide as this column (SidebarNav.railAxis), so
            // it is centred there without moving while the rail folds.
            x: control.iconAxis - width / 2 - control.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            width: control.tileSize
            height: control.tileSize
            radius: Theme.radius.sm
            // No fill or hairline: the icon is the picture (InstanceTile).
            color: "transparent"
            lifted: control.hovered
            matte: !control.iconFills || !Theme.solidShell ? "transparent"
                 : Qt.tint(control.surround, control.compact ? "transparent"
                                            : control.down ? Theme.palette.pressedOverlay
                                            : control.hovered ? Theme.palette.hoverOverlay : "transparent")

            Image {
                anchors.centerIn: parent
                width: control.iconFills ? parent.width : control.pictureSize
                height: width
                source: Icons.instanceIcon(control.drawnKey)
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectFit
            }
            KiteNose {
                visible: control.selected && control.compact
                z: 2
                x: tile.width - leg
                y: 0
            }

            // Pulses rather than sitting static, so a glance at the rail
            // says "still running" instead of just "ran once". Ringed in the
            // shell's own surface so it stays a dot over any icon.
            Rectangle {
                visible: control.isRunning
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: -Theme.space.xxs
                width: Theme.space.md
                height: width
                radius: width / 2
                color: Theme.palette.success
                border.width: Theme.space.xxs
                border.color: Theme.palette.surface

                SequentialAnimation on opacity {
                    running: control.isRunning && control.motionEnabled
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 0.45; duration: 900; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.45; to: 1.0; duration: 900; easing.type: Easing.InOutSine }
                }
            }
        }

        Text {
            visible: !control.compact
            anchors.left: parent.left
            anchors.leftMargin: control.labelX - control.leftPadding
            anchors.right: trailing.left
            anchors.rightMargin: Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            text: control.name
            elide: Text.ElideRight
            color: control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: Font.Medium
        }

        Item {
            id: trailing
            visible: !control.compact
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.control.heightSm
            height: width

            IconButton {
                anchors.fill: parent
                size: parent.width
                visible: control.hovered && !control.isRunning
                iconName: "play"
                tip: qsTr("Play")
                focusPolicy: Qt.NoFocus
                onClicked: control.playRequested()
            }
        }
    }
}
