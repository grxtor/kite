// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One choice in the cape picker: a real cape, "No cape" (an empty, struck-through cape slot), or an owned cape
 * whose texture has not resolved (a neutral silhouette). The front strip is a 10x16 region at (1, 1) of the 64x32
 * texture; Image has no source rectangle, so the crop overscales the texture inside a clipped Item. The chosen
 * tile wears a textPrimary outline, never a tint.
 */
Item {
    id: root

    property string label
    property string capeUrl: ""
    property bool selected: false
    // Set only by the picker's "No cape" tile; every other tile is a cape the account owns.
    property bool isNoCapeOption: false
    property bool compact: false
    signal clicked()

    readonly property bool hasCape: root.capeUrl.length > 0
    readonly property int stripWidth: 10
    readonly property int stripHeight: 16
    readonly property int thumbScale: root.compact ? 3 : 4
    readonly property real silhouetteWidth: 0.85
    readonly property real silhouetteHeight: 0.72
    readonly property int compactPadding: Theme.space.sm

    implicitWidth: root.compact ? root.stripWidth * root.thumbScale + root.compactPadding * 2 : 88
    implicitHeight: root.compact ? root.stripHeight * root.thumbScale + root.compactPadding * 2 : 118

    ToolTip.visible: hover.hovered && (root.compact || nameLabel.truncated)
    ToolTip.delay: 400
    ToolTip.text: root.label

    readonly property int selectionWidth: 2

    Accessible.role: Accessible.RadioButton
    Accessible.name: root.label
    Accessible.checked: root.selected

    // The pointer lifts the tile; the selection is the outline alone.
    StateFill {
        anchors.fill: parent
        radius: Theme.radius.md
        hovered: hover.hovered && root.enabled && !root.selected
        border.width: root.selected ? root.selectionWidth : 0
        border.color: Theme.palette.textPrimary
    }

    Item {
        id: thumb
        anchors.top: parent.top
        anchors.topMargin: root.compact ? root.compactPadding : Theme.space.xs
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.stripWidth * root.thumbScale
        height: root.stripHeight * root.thumbScale
        clip: true
        visible: root.hasCape

        // Overscale the texture so its front strip fills this item; clip: true crops the rest.
        readonly property real cropScale: width / root.stripWidth

        Image {
            source: root.capeUrl
            asynchronous: true
            smooth: false
            width: 64 * thumb.cropScale
            height: 32 * thumb.cropScale
            x: -1 * thumb.cropScale
            y: -1 * thumb.cropScale
        }
    }

    // "No cape": the slot a cape would fill, empty and struck through.
    Rectangle {
        anchors.fill: thumb
        visible: root.isNoCapeOption
        radius: Theme.radius.xs
        color: "transparent"
        border.width: 1
        border.color: Theme.neutral.outline

        MeshIcon {
            anchors.centerIn: parent
            iconName: "ban"
            size: Theme.icon.sm
            color: Theme.palette.textTertiary
        }
    }

    // A cloak shape for an owned cape whose texture is not known yet; not an absence, so it must not read as "No
    // cape".
    Canvas {
        id: capeSilhouette
        anchors.centerIn: thumb
        width: thumb.width * root.silhouetteWidth; height: thumb.height * root.silhouetteHeight
        visible: !root.hasCape && !root.isNoCapeOption
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.palette.textTertiary
            ctx.beginPath()
            ctx.moveTo(width * 0.5, 0)
            ctx.lineTo(width * 0.78, height * 0.16)
            ctx.lineTo(width * 0.94, height)
            ctx.quadraticCurveTo(width * 0.5, height * 0.86, width * 0.06, height)
            ctx.lineTo(width * 0.22, height * 0.16)
            ctx.closePath()
            ctx.fill()
        }
        onVisibleChanged: if (visible) requestPaint()
        Component.onCompleted: requestPaint()
        // Theme is a singleton every instance shares, so this Connections repaints every tile on a scheme/mode
        // switch.
        Connections {
            target: Theme
            function onPaletteChanged() { capeSilhouette.requestPaint() }
        }
    }

    Copy {
        id: nameLabel
        visible: !root.compact
        anchors.top: thumb.bottom
        anchors.topMargin: Theme.space.xs
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.space.xxs
        horizontalAlignment: Text.AlignHCenter
        text: root.label
        role: "caption"
        tone: root.selected ? "primary" : "secondary"
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    HoverHandler {
        id: hover
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    TapHandler {
        onTapped: root.clicked()
    }
}
