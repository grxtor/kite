// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

/*
 * A Minecraft player in 3D: a skin on the standard model, with a cape or
 * the elytra on its back, which a drag turns around. The skin editor and
 * the Accounts page show it through Components/SkinPreview.qml, which only
 * loads this file where Qt Quick3D exists and draws (the same condition as
 * the cat, MESHMC_HAS_CAT), and shows the flat render otherwise.
 *
 * The model is the one the widget skin dialog drew with OpenGL
 * (ui/dialogs/skins/render/PlayerScene.cpp): every part a box, sized and
 * placed in skin pixels, its six faces cut from the 64x64 texture in the
 * standard layout -- cubeFaces() below is that renderer's cubeTexCoords().
 * The second layer (hat, jacket, sleeves, trousers) is a slightly larger
 * box over each part, its transparent pixels cut out.
 *
 * Calm on purpose: it holds still at a three-quarter view until dragged,
 * and turns back to it on a double click. showBack() turns it round once,
 * for whoever just put a cape or the elytra on it.
 */
Item {
    id: root

    // The 64x64 texture (image://accountface/skin/... or skinfile/...).
    property url skinTexture
    property bool slim: false
    // The 64x32 cape texture, or empty for no cape.
    property url capeTexture
    readonly property bool hasCape: root.capeTexture.toString().length > 0
    // Wear the elytra instead: in the cape's texture, or plain in
    // `plainColor` without a cape (the game's own elytra texture is not
    // ours to ship), as the flat view's figure is plain.
    property bool elytra: false
    property color plainColor: "grey"

    // How long showBack()/showFront() take, and their easing; set by
    // Components/SkinPreview.qml from the theme (this module has no
    // Theme import of its own).
    property int turnDuration: 0
    property int turnEasing: Easing.OutCubic

    readonly property real restYaw: -25
    property real yaw: restYaw
    property real pitch: 0
    readonly property real maxPitch: 25
    readonly property real halfTurn: 180
    readonly property real fullTurn: 360
    // Degrees turned (and tilted) per pixel dragged.
    readonly property real turnPerPixel: 0.6
    readonly property real tiltPerPixel: 0.3

    // Turns to @p target the short way round from wherever a drag left it.
    function turnTo(target) {
        var from = ((root.yaw - target) % root.fullTurn + root.fullTurn) % root.fullTurn
        if (from > root.halfTurn)
            from -= root.fullTurn
        turnAnimation.stop()
        root.yaw = target + from
        turnAnimation.yawTo = target
        turnAnimation.start()
    }
    function showBack() { root.turnTo(root.restYaw + root.halfTurn) }
    function showFront() { root.turnTo(root.restYaw) }

    ParallelAnimation {
        id: turnAnimation
        property real yawTo: root.restYaw
        NumberAnimation { target: root; property: "yaw"; to: turnAnimation.yawTo; duration: root.turnDuration; easing.type: root.turnEasing }
        NumberAnimation { target: root; property: "pitch"; to: 0; duration: root.turnDuration; easing.type: root.turnEasing }
    }

    // Six faces of a w x h x d box centred at `pos`, as ProceduralMesh
    // arrays, its texture cut from (u, v) in a texW x texH image with the
    // box's own (tw, th, td) pixel size -- the overlay boxes are bigger
    // than the pixels they show. Faces in CubeMesh's order: front (+z),
    // +x, back (-z), -x, bottom, top. `mirrored` reflects the whole box,
    // picture and all, across x = 0, as the game's mirrored parts are; in
    // the data rather than with a negative scale, which would turn its
    // faces inside out for the lighting.
    function cubeFaces(size, pos, u, v, dims, texW, texH, mirrored) {
        var w = dims.x, h = dims.y, d = dims.z
        function rect(x1, y1, x2, y2) {
            return [Qt.vector2d(x1 / texW, 1 - y2 / texH), Qt.vector2d(x2 / texW, 1 - y2 / texH),
                    Qt.vector2d(x2 / texW, 1 - y1 / texH), Qt.vector2d(x1 / texW, 1 - y1 / texH)]
        }
        var top = rect(u + d, v, u + w + d, v + d)
        var bottom = rect(u + w + d, v, u + w * 2 + d, v + d)
        var left = rect(u, v + d, u + d, v + d + h)
        var front = rect(u + d, v + d, u + w + d, v + d + h)
        var right = rect(u + w + d, v + d, u + w + d * 2, v + h + d)
        var back = rect(u + w + d * 2, v + d, u + w * 2 + d * 2, v + h + d)
        // [corner offsets in half-sizes], uvs, normal -- CubeMesh's table.
        var faces = [
            { c: [[-1,-1, 1], [ 1,-1, 1], [-1, 1, 1], [ 1, 1, 1]], t: [front[0], front[1], front[3], front[2]], n: Qt.vector3d(0, 0, 1) },
            { c: [[ 1,-1, 1], [ 1,-1,-1], [ 1, 1, 1], [ 1, 1,-1]], t: [right[0], right[1], right[3], right[2]], n: Qt.vector3d(1, 0, 0) },
            { c: [[ 1,-1,-1], [-1,-1,-1], [ 1, 1,-1], [-1, 1,-1]], t: [back[0], back[1], back[3], back[2]], n: Qt.vector3d(0, 0, -1) },
            { c: [[-1,-1,-1], [-1,-1, 1], [-1, 1,-1], [-1, 1, 1]], t: [left[0], left[1], left[3], left[2]], n: Qt.vector3d(-1, 0, 0) },
            { c: [[-1,-1,-1], [ 1,-1,-1], [-1,-1, 1], [ 1,-1, 1]], t: [bottom[3], bottom[2], bottom[0], bottom[1]], n: Qt.vector3d(0, -1, 0) },
            { c: [[-1, 1, 1], [ 1, 1, 1], [-1, 1,-1], [ 1, 1,-1]], t: [top[0], top[1], top[3], top[2]], n: Qt.vector3d(0, 1, 0) }
        ]
        var flip = mirrored ? -1 : 1
        var positions = [], uvs = [], normals = [], indexes = []
        for (var f = 0; f < faces.length; ++f) {
            var base = positions.length
            for (var i = 0; i < 4; ++i) {
                var c = faces[f].c[i]
                positions.push(Qt.vector3d(flip * (pos.x + c[0] * size.x / 2), pos.y + c[1] * size.y / 2, pos.z + c[2] * size.z / 2))
                uvs.push(faces[f].t[i])
                var n = faces[f].n
                normals.push(Qt.vector3d(flip * n.x, n.y, n.z))
            }
            if (mirrored)
                indexes.push(base, base + 2, base + 1, base + 1, base + 2, base + 3)
            else
                indexes.push(base, base + 1, base + 2, base + 1, base + 3, base + 2)
        }
        return { positions: positions, uvs: uvs, normals: normals, indexes: indexes }
    }

    // A box as a Model. `material` decides the texture.
    component Box: Model {
        id: box
        property vector3d size
        property vector3d at
        property point uv
        property vector3d pixels: size
        property int texWidth: 64
        property int texHeight: 64
        property bool mirrored: false
        readonly property var faces: root.cubeFaces(box.size, box.at, box.uv.x, box.uv.y,
                                                    box.pixels, box.texWidth, box.texHeight, box.mirrored)
        geometry: ProceduralMesh {
            positions: box.faces.positions
            uv0s: box.faces.uvs
            normals: box.faces.normals
            indexes: box.faces.indexes
        }
    }

    // A 2D item drawn into a texture -- the image providers serve QML
    // Images, and nearest filtering keeps the pixels square.
    component PixelTexture: Texture {
        id: pixelTexture
        property url imageSource
        property int pixelWidth: 64
        property int pixelHeight: 64
        generateMipmaps: false
        magFilter: Texture.Nearest
        minFilter: Texture.Nearest
        mipFilter: Texture.None
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
        sourceItem: Image {
            width: pixelTexture.pixelWidth
            height: pixelTexture.pixelHeight
            source: pixelTexture.imageSource
            smooth: false
            mipmap: false
        }
    }

    // Matte, as in the game: no specular highlights at all.
    PrincipledMaterial {
        id: skinMaterial
        roughness: 1
        metalness: 0
        specularAmount: 0
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Mask
        alphaCutoff: 0.5
        baseColorMap: PixelTexture { imageSource: root.skinTexture }
    }
    PrincipledMaterial {
        id: capeMaterial
        roughness: 1
        metalness: 0
        specularAmount: 0
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Mask
        alphaCutoff: 0.5
        baseColorMap: PixelTexture { imageSource: root.capeTexture; pixelHeight: 32 }
    }
    PrincipledMaterial {
        id: plainMaterial
        roughness: 1
        metalness: 0
        specularAmount: 0
        cullMode: PrincipledMaterial.NoCulling
        baseColor: root.plainColor
    }

    View3D {
        id: view
        anchors.fill: parent
        renderMode: View3D.Offscreen
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            // Linear, not the cat's None: None also skips the conversion
            // back to sRGB, which leaves every skin a few shades too dark.
            tonemapMode: SceneEnvironment.TonemapModeLinear
        }

        // The whole figure, 32 skin pixels tall and centred on y = -8,
        // fills the view's height with a little room to spare.
        PerspectiveCamera {
            position: Qt.vector3d(0, -3, 84)
            eulerRotation.x: -3.5
            fieldOfView: 28
            clipNear: 10
            clipFar: 200
        }
        // Fixed to the view, not the player: a key light from above and a
        // little right of the camera, so whatever side is turned towards
        // the viewer (the face, or the cape) is the brightest, and a soft
        // fill from the left.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-30, 10, 0)
            brightness: 0.95
            ambientColor: Qt.rgba(0.22, 0.22, 0.24, 1)
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(-10, -60, 0)
            brightness: 0.25
        }

        Node {
            eulerRotation.x: root.pitch
            eulerRotation.y: root.yaw

            // Base layer. The legs meet edge to edge, as the arms meet the
            // body, not 0.1 in each as in the game: overlapping, each leg's
            // inner side meets the other's back at the same depth, and a
            // bright line shows between them.
            Box { materials: [skinMaterial]; size: Qt.vector3d(8, 8, 8); at: Qt.vector3d(0, 4, 0); uv: Qt.point(0, 0) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(8, 12, 4); at: Qt.vector3d(0, -6, 0); uv: Qt.point(16, 16) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(4, 12, 4); at: Qt.vector3d(-2, -18, -0.1); uv: Qt.point(0, 16) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(4, 12, 4); at: Qt.vector3d(2, -18, -0.1); uv: Qt.point(16, 48) }
            Box {
                materials: [skinMaterial]
                size: Qt.vector3d(root.slim ? 3 : 4, 12, 4)
                at: Qt.vector3d(root.slim ? -5.5 : -6, -6, 0)
                uv: Qt.point(40, 16)
            }
            Box {
                materials: [skinMaterial]
                size: Qt.vector3d(root.slim ? 3 : 4, 12, 4)
                at: Qt.vector3d(root.slim ? 5.5 : 6, -6, 0)
                uv: Qt.point(32, 48)
            }

            // Second layer, half a pixel out on every side.
            Box { materials: [skinMaterial]; size: Qt.vector3d(9, 9, 9); pixels: Qt.vector3d(8, 8, 8); at: Qt.vector3d(0, 4, 0); uv: Qt.point(32, 0) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(8.5, 12.5, 4.5); pixels: Qt.vector3d(8, 12, 4); at: Qt.vector3d(0, -6, 0); uv: Qt.point(16, 32) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(4.5, 12.5, 4.5); pixels: Qt.vector3d(4, 12, 4); at: Qt.vector3d(-2, -18, -0.1); uv: Qt.point(0, 32) }
            Box { materials: [skinMaterial]; size: Qt.vector3d(4.5, 12.5, 4.5); pixels: Qt.vector3d(4, 12, 4); at: Qt.vector3d(2, -18, -0.1); uv: Qt.point(0, 48) }
            Box {
                materials: [skinMaterial]
                size: Qt.vector3d(root.slim ? 3.5 : 4.5, 12.5, 4.5)
                pixels: Qt.vector3d(root.slim ? 3 : 4, 12, 4)
                at: Qt.vector3d(root.slim ? -5.5 : -6, -6, 0)
                uv: Qt.point(40, 32)
            }
            Box {
                materials: [skinMaterial]
                size: Qt.vector3d(root.slim ? 3.5 : 4.5, 12.5, 4.5)
                pixels: Qt.vector3d(root.slim ? 3 : 4, 12, 4)
                at: Qt.vector3d(root.slim ? 5.5 : 6, -6, 0)
                uv: Qt.point(48, 48)
            }

            // The cape: hung from the shoulders behind the back, its
            // picture facing away from the player, the hem a little out.
            Node {
                visible: root.hasCape && !root.elytra
                position: Qt.vector3d(0, 0, -2)
                eulerRotation.y: 180
                Node {
                    eulerRotation.x: -10.8
                    Box {
                        materials: [capeMaterial]
                        size: Qt.vector3d(10, 16, 1)
                        at: Qt.vector3d(0, -8, 0.5)
                        uv: Qt.point(0, 0)
                        texHeight: 32
                    }
                }
            }

            // The elytra: two wings from the upper back, folded into an
            // upside-down V, each cut from the cape texture's wing part.
            // The right one is the left mirrored, as in the game. Each
            // wing tilts back (x) before it swings out (z), the game's
            // order -- one Node per axis, since eulerRotation would apply
            // them the other way round.
            Node {
                visible: root.elytra
                Node {
                    position: Qt.vector3d(5, 0, -2)
                    eulerRotation.z: 15
                    Node {
                        eulerRotation.x: 15
                        Box {
                            materials: [root.hasCape ? capeMaterial : plainMaterial]
                            size: Qt.vector3d(12, 22, 4)
                            pixels: Qt.vector3d(10, 20, 2)
                            at: Qt.vector3d(-5, -10, -1)
                            uv: Qt.point(22, 0)
                            texHeight: 32
                        }
                    }
                }
                Node {
                    position: Qt.vector3d(-5, 0, -2)
                    eulerRotation.z: -15
                    Node {
                        eulerRotation.x: 15
                        Box {
                            materials: [root.hasCape ? capeMaterial : plainMaterial]
                            size: Qt.vector3d(12, 22, 4)
                            pixels: Qt.vector3d(10, 20, 2)
                            at: Qt.vector3d(-5, -10, -1)
                            uv: Qt.point(22, 0)
                            texHeight: 32
                            mirrored: true
                        }
                    }
                }
            }
        }
    }

    // Drag to turn (and tilt a little); double click to face front again.
    DragHandler {
        id: turn
        target: null
        property real startYaw: 0
        property real startPitch: 0
        // Not handed over to a Flickable around it (the skin editor
        // scrolls in a small window) once a turn has started.
        grabPermissions: PointerHandler.TakeOverForbidden
        onActiveChanged: if (active) {
            turnAnimation.stop()
            startYaw = root.yaw
            startPitch = root.pitch
        }
        onTranslationChanged: {
            root.yaw = startYaw + translation.x * root.turnPerPixel
            root.pitch = Math.max(-root.maxPitch, Math.min(root.maxPitch, startPitch + translation.y * root.tiltPerPixel))
        }
    }
    TapHandler {
        onDoubleTapped: root.showFront()
    }
}
