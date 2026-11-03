// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One recently played instance in the sidebar: icon on a small neutral tile and,
 * in the labelled sidebar, the name and a hover play button. On the icon rail
 * (`compact`) the name is a tooltip and there is no play button.
 */
AbstractButton {
    id: control

    // Filled from the instance model's roles; a caller redeclaring them would shadow these and
    // leave this file reading blanks.
    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning

    property bool compact: false

    // Decorative loops stop when the window is inactive or "Reduce motion" is on.
    readonly property bool motionEnabled: Qt.application.state === Qt.ApplicationActive && !Theme.motion.reduced

    signal playRequested()

    implicitHeight: Theme.control.height
    implicitWidth: 200
    hoverEnabled: true

    Accessible.name: name

    ToolTip {
        visible: control.compact && control.hovered
        delay: 400
        text: control.name
        x: control.width + Theme.space.md
        y: Math.round((control.height - implicitHeight) / 2)
    }

    property int railSquare: Theme.control.height
    property int cornerRadius: Theme.radius.md
    property int iconAxis: Theme.space.md + Theme.icon.md / 2
    property int labelX: control.iconAxis + control.tileSize / 2 + Theme.space.sm
    readonly property int tileSize: Theme.control.height
    readonly property int pictureSize: Math.round(control.tileSize * 0.86)
    // An opaque square icon (Icons.fillsTile) fills the tile, its corners rounded in `surround`:
    // the colour the rail shows around the tile.
    readonly property bool iconFills: Icons.fillsTile(control.drawnKey)
    readonly property string drawnKey: Icons.displayKey(control.iconKey, control.instanceId)
    property color surround: "transparent"
    // The open instance: on the folded rail its tile lights the kite's nose instead of SidebarNav's
    // selection square framing it.
    property bool selected: false

    background: StateFill {
        width: control.compact ? control.railSquare : control.width
        height: control.compact ? control.railSquare : control.height
        x: control.compact ? control.iconAxis - width / 2 : 0
        y: control.compact ? (control.height - height) / 2 : 0
        radius: control.cornerRadius
        // On the icon rail the tile itself answers the pointer; a square behind it made a box
        // around a box.
        hovered: !control.compact && control.hovered
        down: !control.compact && control.down
    }

    contentItem: Item {
        KiteShape {
            id: tile
            // At the same x in both modes: the icon rail is exactly as wide as this column
            // (SidebarNav.railAxis), so it stays put while the rail folds.
            x: control.iconAxis - width / 2 - control.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            width: control.tileSize
            height: control.tileSize
            radius: Theme.radius.sm
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

            // Pulses while running; ringed in the shell's surface so it stays a dot over any icon.
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
                    NumberAnimation { from: 1.0; to: 0.45; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.45; to: 1.0; duration: Theme.motion.pulse; easing.type: Easing.InOutSine }
                }
            }
        }

        Copy {
            visible: !control.compact
            anchors.left: parent.left
            anchors.leftMargin: control.labelX - control.leftPadding
            anchors.right: trailing.left
            anchors.rightMargin: Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            text: control.name
            elide: Text.ElideRight
            color: control.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
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

    // Files dropped on the tile are added to this instance (Main.qml dropOnInstance).
    InstanceDropTarget {
        anchors.fill: parent
        z: 10
        instanceId: control.instanceId
        radius: Theme.radius.md
    }
}
