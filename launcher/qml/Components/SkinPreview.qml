// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A skin on a player, as large as the box it is given: in 3D, turned by dragging,
 * where the build has Qt Quick3D (`skin3DAvailable`) and the window draws through
 * a 3D graphics API; otherwise the flat front view (`flatSource`) over the
 * neutral figure. A soft light sits behind either so a dark skin does not melt
 * into a dark surface. showBack()/showFront() turn the 3D player round (no-ops
 * in the flat view).
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
    // Whether launcher/qml/Skin3D was built (Main.qml's skin3DAvailable).
    property bool skin3DAvailable: false

    readonly property bool hasSkin: root.skinTexture.toString().length > 0
    // Qt Quick3D renders nothing through the software renderer (offscreen review snapshots use it),
    // so the flat view is used there too.
    readonly property bool threeD: root.skin3DAvailable && root.hasSkin
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
        source: active ? "qrc:/qt/qml/Kite/Skin3D/SkinPreview3D.qml" : ""
        onLoaded: {
            item.skinTexture = Qt.binding(() => root.skinTexture)
            item.slim = Qt.binding(() => root.slim)
            item.capeTexture = Qt.binding(() => root.capeTexture)
            item.elytra = Qt.binding(() => root.elytra)
            item.plainColor = Qt.binding(() => silhouette.color)
            item.turnDuration = Qt.binding(() => SettingsStore.bool("UiReduceMotion") ? 0 : Theme.motion.slow)
            item.turnEasing = Theme.motion.easing
            item.shadowColor = Qt.binding(() => Theme.palette.shadow)
        }
    }

    Item {
        id: flat
        visible: !preview3d.active || preview3d.status === Loader.Error
        readonly property real figureHeight: Math.min(root.height, root.width * 2)
        width: figureHeight / 2
        height: figureHeight
        anchors.centerIn: parent

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
