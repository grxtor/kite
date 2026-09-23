// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * One choice in the skin/cape editor's cape picker: either a real cape, the
 * "No cape" choice (which has no texture, by definition -- an "x"), or a
 * cape the account owns whose texture has not resolved yet (a neutral cape
 * silhouette, never the same "x" as "No cape": the account still owns that
 * cape, it just cannot be drawn yet).
 *
 * A cape texture is 64x32, with its front-facing strip -- the part worth
 * showing in a small tile -- an 10x16 region at (1, 1) (the same region
 * SkinManageDialog's own thumbnail crops, see kCapeFrontRegion in
 * ui/dialogs/skins/SkinManageDialog.cpp). QML's Image has no source
 * rectangle of its own, so the crop is done by overscaling the whole
 * texture inside a clipped Item instead: the image is drawn at a size and
 * offset that puts just that region inside the clip, and clip: true throws
 * the rest away.
 */
Item {
    id: root

    property string label
    property string capeUrl: ""
    property bool selected: false
    // Set only by the picker's dedicated "No cape" tile -- see
    // SkinCapeEditor.qml. Every other tile is a cape the account owns,
    // whether or not its texture has resolved yet.
    property bool isNoCapeOption: false
    signal clicked()

    readonly property bool hasCape: root.capeUrl.length > 0

    implicitWidth: 72
    implicitHeight: 100

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.md
        color: root.selected ? Theme.palette.accentSubtle : Theme.palette.surface
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent : Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Item {
        id: thumb
        anchors.top: parent.top
        anchors.topMargin: Theme.space.xs
        anchors.horizontalCenter: parent.horizontalCenter
        width: 40
        height: 64
        clip: true
        visible: root.hasCape

        // Scale the whole 64x32 texture up so its 10x16 front strip at
        // (1, 1) fills this item, then let clip: true crop away the rest.
        readonly property real cropScale: width / 10

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

    MeshIcon {
        anchors.centerIn: thumb
        visible: !root.hasCape && root.isNoCapeOption
        iconName: "x"
        size: Theme.icon.md
        color: Theme.palette.textTertiary
    }

    // A generic cloak shape for a cape the account owns but whose texture is
    // not known yet -- there is no such icon in icons/, and unlike "No
    // cape" this is not an absence, so it must not read as one.
    Canvas {
        id: capeSilhouette
        anchors.centerIn: thumb
        width: 34; height: 46
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
        // Theme.dark isn't a per-instance signal, but Theme itself is a
        // singleton every instance shares, so this Connections is enough
        // to repaint every tile on a scheme/mode switch.
        Connections {
            target: Theme
            function onPaletteChanged() { capeSilhouette.requestPaint() }
        }
    }

    Text {
        anchors.top: thumb.bottom
        anchors.topMargin: Theme.space.xs
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.space.xxs
        horizontalAlignment: Text.AlignHCenter
        text: root.label
        elide: Text.ElideRight
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
    }

    TapHandler {
        onTapped: root.clicked()
    }
}
