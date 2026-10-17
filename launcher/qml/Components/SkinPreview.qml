// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A skin on a player, as large as the box it is given: in 3D, turned by
 * dragging, where the build has Qt Quick3D (the cat's module,
 * MeshMC_ENABLE_CAT -- `catAvailable`) and the window draws through a 3D
 * graphics API; otherwise the flat front view (`flatSource`, e.g.
 * image://accountface/body/<id>) over the neutral figure, which only holds
 * the place while there is no texture yet (an account without a skin of
 * its own is drawn in the game's default one by AccountFaceProvider, once
 * that has downloaded). Used by the skin editor and
 * the Accounts page's hero. showBack()/showFront() turn the 3D player
 * round (the flat view has no back, so there they do nothing).
 *
 * Behind either view, a soft light: a black or dark skin on a dark surface
 * would otherwise melt into it, only its light parts showing.
 */
Item {
    id: root

    // The 64x64 skin texture (image://accountface/skin/<id> or
    // skinfile/<path>); empty when there is no skin to show.
    property url skinTexture
    // The flat front view for the 2D fallback; may be empty.
    property url flatSource
    property bool slim: false
    // The cape's 64x32 texture, or empty.
    property url capeTexture
    property bool elytra: false
    // Whether launcher/qml/Cat was built (Main.qml's catAvailable).
    property bool catAvailable: false

    readonly property bool hasSkin: root.skinTexture.toString().length > 0
    // Qt Quick3D renders nothing through the software renderer (and the
    // offscreen review snapshots use it), so the flat view is the honest
    // choice there too.
    readonly property bool threeD: root.catAvailable && root.hasSkin
                                   && GraphicsInfo.api !== GraphicsInfo.Software
                                   && GraphicsInfo.api !== GraphicsInfo.Unknown

    // The light behind the player: the palette's lightest colour, strongest
    // behind the figure and gone at the edges. Faint over the dark
    // surfaces, where it does the work; stronger over the light ones,
    // where it only brightens the sunken grey a little.
    readonly property color glowColor: Theme.dark ? Theme.palette.textPrimary : Theme.palette.surfaceRaised
    readonly property real glowOpacity: Theme.dark ? 0.4 : 0.8
    // Its fade, (1 - t^flatness)^2 from the brightest point (t = 0) to
    // the edge (t = 1), soft all the way with no ring where it turns;
    // drawn as that many gradient stops. The brightest point sits below
    // the middle by glowFocus of the radius, so the ground and the
    // shadow under the feet are lit too.
    readonly property real glowFlatness: 2
    readonly property int glowStops: 16
    readonly property real glowFocus: 0.25

    function showBack() {
        if (preview3d.item)
            preview3d.item.showBack()
    }
    function showFront() {
        if (preview3d.item)
            preview3d.item.showFront()
    }

    Canvas {
        id: glow
        anchors.fill: parent
        property color color: root.glowColor
        property real opacityAtMiddle: root.glowOpacity
        onColorChanged: requestPaint()
        onOpacityAtMiddleChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        // A circle of the box's width, stretched to its height.
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const radius = width / 2
            ctx.translate(radius, height / 2)
            ctx.scale(1, height / width)
            const light = ctx.createRadialGradient(0, radius * root.glowFocus, 0, 0, 0, radius)
            for (let i = 0; i <= root.glowStops; ++i) {
                const t = i / root.glowStops
                const fade = 1 - Math.pow(t, root.glowFlatness)
                light.addColorStop(t, Qt.alpha(color, opacityAtMiddle * fade * fade))
            }
            ctx.fillStyle = light
            ctx.fillRect(-radius, -radius, width, width)
        }
    }

    Loader {
        id: preview3d
        anchors.fill: parent
        active: root.threeD
        source: active ? "qrc:/qt/qml/MeshMC/Cat/SkinPreview3D.qml" : ""
        onLoaded: {
            item.skinTexture = Qt.binding(() => root.skinTexture)
            item.slim = Qt.binding(() => root.slim)
            item.capeTexture = Qt.binding(() => root.capeTexture)
            item.elytra = Qt.binding(() => root.elytra)
            // An elytra without a cape is plain, in the flat figure's colour.
            item.plainColor = Qt.binding(() => silhouette.color)
            // A cut instead of a turn with reduced motion on.
            item.turnDuration = Qt.binding(() => SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.slow)
            item.turnEasing = Theme.motion.easing
            item.shadowColor = Qt.binding(() => Theme.palette.shadow)
        }
    }

    // The flat view: a 1:2 figure box, centred, as large as fits.
    Item {
        id: flat
        visible: !preview3d.active || preview3d.status === Loader.Error
        readonly property real figureHeight: Math.min(root.height, root.width * 2)
        width: figureHeight / 2
        height: figureHeight
        anchors.centerIn: parent

        // Under the render while it loads, and in its place without a
        // skin (the render then comes back transparent).
        SkinSilhouette {
            id: silhouette
            anchors.fill: parent
            visible: !root.hasSkin || flatImage.status !== Image.Ready
        }
        Image {
            id: flatImage
            anchors.fill: parent
            visible: root.hasSkin
            source: root.hasSkin ? root.flatSource : ""
            sourceSize: Qt.size(flat.width * 2, flat.height * 2)
            fillMode: Image.PreserveAspectFit
            smooth: false
        }
    }
}
