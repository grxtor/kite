// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Your Minecraft name where friends find it (the Friends page, Settings):
 * plain text that copies itself on a click, so it can be sent to someone.
 * The copy glyph shows while the pointer or the keyboard is on it, then a
 * check and "Copied" for a moment. Its room is always kept, so the name
 * does not move. With no name (`value` empty) it only says `emptyText`.
 *
 * `glyphFirst` puts the glyph before the name, so the name itself ends on
 * the edge it is aligned to (Settings' right-hand column).
 *
 * `chip` is the invitation form (the empty Friends tab): a bordered button
 * with the glyph always shown, which answers by saying "Copied" where the
 * name was, on a width that fits both so nothing moves.
 */
AbstractButton {
    id: root

    property string value
    // What it says, with nothing to copy, while there is no name yet.
    property string emptyText
    property int pixelSize: Theme.type.body.pixelSize
    property int weight: Font.Normal
    property color textColor: Theme.palette.textPrimary
    property bool glyphFirst: false
    property bool chip: false
    property bool copied: false

    // How long it says "Copied", and how long the pointer rests before the tip.
    readonly property int copiedMs: 1500
    readonly property int tipDelayMs: 500
    readonly property bool empty: root.value.length === 0
    readonly property string shown: root.empty ? root.emptyText : root.value
    readonly property bool lit: !root.empty && (root.hovered || root.visualFocus || root.copied)
    // The name's room; the chip's also fits "Copied", so it does not resize.
    readonly property real labelWidth: root.chip ? Math.max(nameMetrics.advanceWidth, copiedMetrics.advanceWidth)
                                                 : nameMetrics.advanceWidth

    enabled: !root.empty
    hoverEnabled: true
    padding: 0
    leftPadding: root.chip ? Theme.space.lg : 0
    rightPadding: root.chip ? Theme.space.md : 0
    spacing: root.chip ? Theme.space.md : Theme.space.sm
    implicitWidth: root.leftPadding + root.labelWidth + root.spacing + Theme.icon.sm + root.rightPadding
    implicitHeight: root.chip ? Theme.control.height : Math.max(nameMetrics.height, Theme.icon.sm)
    Accessible.name: qsTr("Copy your name, %1").arg(root.value)
    onClicked: {
        clipboard.copyText(root.value)
        root.copied = true
        copiedTimer.restart()
    }

    ClipboardText { id: clipboard }
    Timer {
        id: copiedTimer
        interval: root.copiedMs
        onTriggered: root.copied = false
    }
    TextMetrics {
        id: nameMetrics
        font.family: Theme.font.family
        font.pixelSize: root.pixelSize
        font.weight: root.weight
        text: root.shown
    }
    TextMetrics {
        id: copiedMetrics
        font: nameMetrics.font
        text: qsTr("Copied")
    }

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    // The chip says it in its own words.
    ToolTip.visible: !root.chip && !root.empty && (root.hovered || root.visualFocus)
    ToolTip.delay: root.copied ? 0 : root.tipDelayMs
    ToolTip.text: root.copied ? qsTr("Copied") : qsTr("Copy your name")

    background: Rectangle {
        radius: root.chip ? Theme.radius.md : Theme.radius.sm
        color: !root.chip ? "transparent"
             : root.down ? Theme.palette.pressedOverlay
             : root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        border.width: root.visualFocus ? Theme.focus.width : root.chip ? 1 : 0
        border.color: root.visualFocus ? Theme.palette.focusRing
                    : root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        anchors.margins: root.chip ? 0 : -Theme.focus.offset
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }
    contentItem: Item {
        Text {
            id: nameLabel
            anchors.verticalCenter: parent.verticalCenter
            x: root.glyphFirst ? Theme.icon.sm + root.spacing : 0
            width: Math.min(root.labelWidth, root.availableWidth - root.spacing - Theme.icon.sm)
            text: root.chip && root.copied ? qsTr("Copied") : root.shown
            elide: Text.ElideRight
            color: root.copied ? Theme.palette.success : root.textColor
            font: nameMetrics.font
        }
        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            x: root.glyphFirst ? 0 : nameLabel.width + root.spacing
            iconName: root.copied ? "check" : "copy"
            size: Theme.icon.sm
            color: root.copied ? Theme.palette.success
                 : root.chip && root.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            // The chip's glyph is what says it can be pressed.
            opacity: root.chip || root.lit ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
    }
}
