// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A skin on a player, as large as the box it is given: in 3D, turned by
 * dragging, where the build has Qt Quick3D (the cat's module,
 * Kite_ENABLE_CAT -- `catAvailable`) and the window draws through a 3D
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

    function showBack() {
        if (preview3d.item)
            preview3d.item.showBack()
    }
    function showFront() {
        if (preview3d.item)
            preview3d.item.showFront()
    }

    Loader {
        id: preview3d
        anchors.fill: parent
        active: root.threeD
        source: active ? "qrc:/qt/qml/Kite/Cat/SkinPreview3D.qml" : ""
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
