// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick3D

/*
 * The cat's skeleton: the parts of assets/cat.glb (see assets/CREDITS.md)
 * regrouped around real joints so they can move.
 *
 * The model as exported puts every leg's pivot at the head, which is fine
 * for a still pose and useless for walking. Here each part hangs from its
 * own joint instead -- neck, ear bases, hips and shoulders, tail base and
 * the bend between the two tail segments -- and the pose is driven by a
 * handful of plain numbers (walkPhase, loaf, sleep, headYaw, ...) that
 * CatOverlay.qml animates. Coordinates are the model's own: 1/16 of a unit
 * per texture pixel, y up, the head towards -z; `px()` keeps the joint
 * positions readable in those pixels.
 *
 * Parts: body (object_0), head (object_1) with its ears (object_2/3) and
 * nose (object_4), legs front-left/front-right/back-left/back-right
 * (object_5..8), tail tip (object_9) and tail base (object_10).
 */
Node {
    id: rig

    // The coat, a 64x64 texture in the model's own UV layout.
    property url texture: "textures/cat_calico.png"

    // -- Pose inputs, all driven from CatOverlay.qml ------------------------
    // Walk cycle angle (radians, grows while walking) and how much of the
    // walk shows (0 = standing, 1 = full stride), so starting and stopping
    // blend instead of snapping.
    property real walkPhase: 0
    property real walkAmount: 0
    // Where the head looks, in degrees: yaw left/right, pitch up/down.
    property real headYaw: 0
    property real headPitch: 0
    // 0..1 lying down with the legs folded under ("loaf"), and 0..1 asleep
    // on top of that (head down and turned, tail curled round).
    property real loaf: 0
    property real sleep: 0
    // 0..1 the front-down, rear-up stretch.
    property real stretch: 0
    // Slow tail sway (degrees) for idling, added on top of the walk's own.
    property real tailSway: 0
    // 0..1 one ear flick.
    property real earTwitch: 0
    // 0..1 breathing, a tiny lift of the chest.
    property real breath: 0

    function px(v) { return v / 16 }

    readonly property real stride: Math.sin(rig.walkPhase) * 30 * rig.walkAmount
    readonly property real bob: Math.abs(Math.sin(rig.walkPhase)) * 0.35 * rig.walkAmount
    // Lying down, the legs fold flat and the whole body drops by their
    // length (4 px) minus a little so the belly rests on the ground.
    readonly property real drop: 3.4 * Math.max(rig.loaf, rig.sleep)
    readonly property real folded: 82 * Math.max(rig.loaf, rig.sleep)

    PrincipledMaterial {
        id: coat
        roughness: 1
        metalness: 0
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Mask
        alphaCutoff: 0.05
        baseColorMap: Texture {
            source: rig.texture
            generateMipmaps: false
            magFilter: Texture.Nearest
            minFilter: Texture.Nearest
            mipFilter: Texture.None
            tilingModeHorizontal: Texture.ClampToEdge
            tilingModeVertical: Texture.ClampToEdge
        }
    }

    // Everything below is laid out in the model's coordinates; this moves
    // the middle of the body (x -1 px, z 3 px) onto the rig's origin, and
    // lowers it when lying down or lifts it for the walk's bob.
    Node {
        position: Qt.vector3d(rig.px(1), rig.px(rig.bob - rig.drop), rig.px(-3))

        // Body, pivoting at the back hips for the stretch.
        Node {
            readonly property vector3d joint: Qt.vector3d(rig.px(-1), rig.px(4), rig.px(10))
            id: body
            position: joint
            eulerRotation.x: -14 * rig.stretch
            scale: Qt.vector3d(1, 1 + 0.02 * rig.breath, 1)
            Node {
                position: body.joint.times(-1)
                Model { source: "model/meshes/object_0_mesh.mesh"; materials: [coat] }
            }
        }

        // Head, pivoting at the neck; ducks with the stretch, tucks in sleep.
        Node {
            readonly property vector3d joint: Qt.vector3d(rig.px(-1), rig.px(8.5), rig.px(-5))
            id: head
            position: joint.plus(Qt.vector3d(0, rig.px(-3 * rig.stretch - 0.8 * rig.sleep), 0))
            eulerRotation: Qt.vector3d(rig.headPitch - 18 * rig.sleep + 10 * rig.stretch,
                                       rig.headYaw + 34 * rig.sleep,
                                       -6 * rig.sleep)
            Node {
                position: head.joint.times(-1)
                Model { source: "model/meshes/object_1_mesh.mesh"; materials: [coat] }
                Model { source: "model/meshes/object_4_mesh.mesh"; materials: [coat] }
                Node {
                    readonly property vector3d joint: Qt.vector3d(rig.px(-2.75), rig.px(11), rig.px(-6))
                    id: earLeft
                    position: joint
                    eulerRotation.z: 22 * rig.earTwitch
                    Node {
                        position: earLeft.joint.times(-1)
                        Model { source: "model/meshes/object_2_mesh.mesh"; materials: [coat] }
                    }
                }
                Node {
                    readonly property vector3d joint: Qt.vector3d(rig.px(0.75), rig.px(11), rig.px(-6))
                    id: earRight
                    position: joint
                    Node {
                        position: earRight.joint.times(-1)
                        Model { source: "model/meshes/object_3_mesh.mesh"; materials: [coat] }
                    }
                }
            }
        }

        // Legs hang from their hips/shoulders; the mesh origins already sit
        // there, so no counter-offset is needed. Diagonal pairs move
        // together, like a real trot; lying down folds front legs forward
        // and back legs back.
        Node {
            position: Qt.vector3d(rig.px(-2.25), rig.px(4), rig.px(-2))
            eulerRotation.x: rig.stride + rig.folded + 30 * rig.stretch
            Model { source: "model/meshes/object_5_mesh.mesh"; materials: [coat] }
        }
        Node {
            position: Qt.vector3d(rig.px(0.25), rig.px(4), rig.px(-2))
            eulerRotation.x: -rig.stride + rig.folded + 30 * rig.stretch
            Model { source: "model/meshes/object_6_mesh.mesh"; materials: [coat] }
        }
        Node {
            position: Qt.vector3d(rig.px(-2.25), rig.px(4), rig.px(9))
            eulerRotation.x: -rig.stride - rig.folded
            Model { source: "model/meshes/object_7_mesh.mesh"; materials: [coat] }
        }
        Node {
            position: Qt.vector3d(rig.px(0.25), rig.px(4), rig.px(9))
            eulerRotation.x: rig.stride - rig.folded
            Model { source: "model/meshes/object_8_mesh.mesh"; materials: [coat] }
        }

        // Tail: the base segment pivots where it leaves the body and carries
        // the tip, which bends again at its own joint -- so a sway ripples
        // down the tail instead of swinging it like a stick. Asleep, it
        // curls forward along the body.
        Node {
            readonly property vector3d joint: Qt.vector3d(0.03125, 0.59375, 0.6875)
            id: tailBase
            position: joint
            eulerRotation: Qt.vector3d(-12 * rig.loaf + 16 * rig.stretch,
                                       rig.tailSway + Math.sin(rig.walkPhase * 0.5) * 10 * rig.walkAmount
                                       + 70 * rig.sleep,
                                       0)
            // The base segment's own tilt from the export (30 degrees about x).
            Node {
                rotation: Qt.quaternion(0.965926, 0.258819, 0, 0)
                Model { source: "model/meshes/object_10_mesh.mesh"; materials: [coat] }
            }
            Node {
                readonly property vector3d joint: Qt.vector3d(rig.px(-0.75), rig.px(5.75), rig.px(17))
                id: tailTip
                position: joint.minus(tailBase.joint)
                eulerRotation: Qt.vector3d(8 * rig.loaf,
                                           rig.tailSway * 0.8
                                           + Math.sin(rig.walkPhase * 0.5 - 0.8) * 14 * rig.walkAmount
                                           + 60 * rig.sleep,
                                           0)
                Node {
                    position: tailTip.joint.times(-1)
                    Model { source: "model/meshes/object_9_mesh.mesh"; materials: [coat] }
                }
            }
        }
    }
}
