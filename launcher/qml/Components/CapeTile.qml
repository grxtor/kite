// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
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
 *
 * `compact` is the small tile of the Accounts page's hero: the cape alone,
 * its name shown on hover instead of under it.
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
    property bool compact: false
    signal clicked()

    readonly property bool hasCape: root.capeUrl.length > 0
    // The cape's front strip is 10x16 texture pixels; the thumbnail shows
    // it at thumbScale screen pixels per texture pixel.
    readonly property int stripWidth: 10
    readonly property int stripHeight: 16
    readonly property int thumbScale: root.compact ? 3 : 4
    // The cloak shape for a cape not drawn yet, as a share of the thumbnail.
    readonly property real silhouetteWidth: 0.85
    readonly property real silhouetteHeight: 0.72
    readonly property int compactPadding: Theme.space.sm

    implicitWidth: root.compact ? root.stripWidth * root.thumbScale + root.compactPadding * 2 : 72
    implicitHeight: root.compact ? root.stripHeight * root.thumbScale + root.compactPadding * 2 : 100

    ToolTip.visible: root.compact && hover.hovered
    ToolTip.delay: 400
    ToolTip.text: root.label

    Rectangle {
        id: card
        anchors.fill: parent
        radius: Theme.radius.md
        color: root.selected ? Theme.palette.accentSubtle : Theme.palette.surface
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : hover.hovered && root.enabled ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
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

        // Scale the whole 64x32 texture up so its 10x16 front strip at
        // (1, 1) fills this item, then let clip: true crop away the rest.
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
        // Theme.dark isn't a per-instance signal, but Theme itself is a
        // singleton every instance shares, so this Connections is enough
        // to repaint every tile on a scheme/mode switch.
        Connections {
            target: Theme
            function onPaletteChanged() { capeSilhouette.requestPaint() }
        }
    }

    Text {
        visible: !root.compact
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

    HoverHandler {
        id: hover
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    TapHandler {
        onTapped: root.clicked()
    }
}
