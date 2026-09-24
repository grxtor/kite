// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick3D
import MeshMC.Components

/*
 * The launcher's cat (a nod to MultiMC's): a small 3D Minecraft cat that
 * lives on the bottom edge of the page area -- the top of the play bar is
 * its floor -- and wanders along it, sits, loafs, sleeps, stretches and
 * looks at the pointer. Click it to pet it; double-click and it hops.
 *
 * Loaded by Main.qml's catLoader over the page area. Only the cat itself
 * takes clicks (catMouse); the pointer tracking is a passive HoverHandler,
 * so nothing under the overlay stops working.
 *
 * Cost: the View3D is tiny, and it only redraws while something moves.
 * Walking is the one continuous animation; idling sways the tail a few
 * times and stops, sleeping breathes in coarse steps a few times a second,
 * and everything stops -- the cat fades out -- while the window is inactive,
 * a dialog is open or "Reduce motion" is on.
 */
Item {
    id: root
    anchors.fill: parent

    // "" runs the cat normally; "walk", "idle", "loaf", "sleep", "stretch"
    // or "pet" freezes it in that pose at a fixed spot, for review
    // snapshots (MESHMC_QML_ROUTE's "cat=<state>" step, see Main.qml).
    property string demo: ""

    readonly property var variants: ["calico", "ginger", "black", "white", "siamese"]
    readonly property string variant: {
        const v = SettingsStore.string("CatVariant")
        return root.variants.indexOf(v) >= 0 ? v : "calico"
    }

    // -- Pausing ------------------------------------------------------------
    readonly property bool reduceMotion: SettingsStore.bool("UiReduceMotion")
    readonly property bool windowActive: root.Window.window ? root.Window.active : true
    // Every open Popup/Dialog is reparented into the window's overlay, so
    // counting its children says whether one is open without Main.qml
    // naming them all.
    readonly property int openPopups: Overlay.overlay ? Overlay.overlay.children.length : 0
    readonly property bool paused: root.demo === ""
                                   && (!root.windowActive || root.reduceMotion || root.openPopups > 0)

    opacity: root.paused ? 0 : 1
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }

    // -- Where the cat is ---------------------------------------------------
    // The stage's horizontal centre, and the range it may walk in (clear of
    // the window edges).
    readonly property real minX: 70
    readonly property real maxX: Math.max(root.minX + 1, root.width - 90)
    property real catX: root.width * 0.72
    // Facing: -1 left, 1 right. The model turns a little towards the viewer
    // (115 degrees rather than 90) so its face shows while it walks.
    property int facing: -1
    property real facingYaw: root.facing > 0 ? -115 : 115
    Behavior on facingYaw { NumberAnimation { duration: 280; easing.type: Easing.OutQuad } }

    // -- What the cat is doing ----------------------------------------------
    // walk, idle, loaf, sleep, stretch; "pet" and "hop" are short
    // interruptions that return to idle.
    property string action: "idle"
    property int idleRounds: 0
    // Bumped on every pet, to start the hearts (see the hearts Repeater).
    property int petPulse: 0

    function randomBetween(lo, hi) { return lo + Math.random() * (hi - lo) }

    function setAction(next) {
        root.action = next
        rig.walkAmount = next === "walk" ? 1 : 0
        rig.loaf = next === "loaf" || next === "sleep" ? 1 : 0
        rig.sleep = next === "sleep" ? 1 : 0
        rig.stretch = next === "stretch" ? 1 : 0
        if (next !== "sleep")
            rig.breath = 0
        if (next === "idle" || next === "loaf")
            tailSway.restart()
        brain.interval = next === "sleep" ? root.randomBetween(18000, 32000)
                       : next === "loaf" ? root.randomBetween(6000, 11000)
                       : next === "stretch" ? 1600
                       : root.randomBetween(2500, 6000)
        brain.restart()
    }

    function walkTo(x) {
        const target = Math.max(root.minX, Math.min(root.maxX, x))
        const distance = target - root.catX
        if (Math.abs(distance) < 12) {
            root.setAction("idle")
            return
        }
        root.facing = distance > 0 ? 1 : -1
        stroll.to = target
        stroll.duration = Math.abs(distance) / 55 * 1000
        root.setAction("walk")
        brain.stop()
        stroll.restart()
    }

    // Picks the next thing to do when the current one runs out.
    function decide() {
        if (root.demo !== "")
            return
        if (root.action === "sleep") {
            root.setAction("stretch")
            return
        }
        if (root.action === "walk" || root.action === "stretch" || root.action === "loaf") {
            root.idleRounds = 0
            root.setAction("idle")
            return
        }
        root.idleRounds += 1
        const roll = Math.random()
        if (root.idleRounds >= 4 && roll < 0.5)
            root.setAction(roll < 0.3 ? "sleep" : "loaf")
        else if (roll < 0.55)
            root.walkTo(root.catX + root.randomBetween(-320, 320))
        else if (roll < 0.7)
            root.setAction("loaf")
        else
            root.setAction("idle")
    }

    // Started and stopped by hand (setAction, walkTo, pet); the pause below
    // only suspends it.
    Timer {
        id: brain
        onTriggered: root.decide()
    }
    onPausedChanged: {
        if (root.paused)
            brain.stop()
        else if (root.demo === "" && root.action !== "walk")
            brain.restart()
    }

    NumberAnimation {
        id: stroll
        target: root
        property: "catX"
        easing.type: Easing.Linear
        paused: root.paused && running
        onFinished: if (root.demo === "") root.setAction("idle")
    }

    // The walk cycle, only while walking.
    NumberAnimation {
        target: rig
        property: "walkPhase"
        from: 0
        to: Math.PI * 2
        duration: 560
        loops: Animation.Infinite
        running: root.action === "walk" && !root.paused && root.demo === ""
    }

    // A few lazy tail swishes whenever the cat settles, then stillness.
    SequentialAnimation {
        id: tailSway
        loops: 3
        running: false
        paused: root.paused && running
        NumberAnimation { target: rig; property: "tailSway"; to: 14; duration: 900; easing.type: Easing.InOutSine }
        NumberAnimation { target: rig; property: "tailSway"; to: -10; duration: 1100; easing.type: Easing.InOutSine }
        NumberAnimation { target: rig; property: "tailSway"; to: 0; duration: 700; easing.type: Easing.InOutSine }
    }

    // Asleep: breathing in three coarse steps, like everything else in a
    // blocky world -- and a redraw only a few times a second.
    Timer {
        interval: 420
        repeat: true
        running: root.action === "sleep" && !root.paused
        property int step: 0
        onTriggered: {
            step = (step + 1) % 6
            rig.breath = [0, 0.5, 1, 1, 0.5, 0][step]
        }
    }

    // An ear flick now and then while awake and still.
    Timer {
        interval: 4200
        repeat: true
        running: (root.action === "idle" || root.action === "loaf") && !root.paused
        onTriggered: if (Math.random() < 0.45) earFlick.restart()
    }
    SequentialAnimation {
        id: earFlick
        NumberAnimation { target: rig; property: "earTwitch"; to: 1; duration: 70 }
        NumberAnimation { target: rig; property: "earTwitch"; to: 0; duration: 160 }
    }

    // -- Looking at the pointer ---------------------------------------------
    // Passive: follows the pointer anywhere over the page without taking a
    // single click from what is underneath.
    HoverHandler {
        id: pointer
        enabled: !root.paused
    }
    readonly property real headX: stage.x + stage.width / 2 + root.facing * 40
    readonly property real headY: stage.y + stage.height * 0.45
    readonly property bool watching: pointer.hovered && root.action !== "sleep" && root.action !== "walk"
                                     && Math.abs(pointer.point.position.x - root.headX) < 360
    Binding {
        target: rig
        property: "headYaw"
        value: {
            if (!root.watching)
                return 0
            // Towards the viewer when the pointer is near, towards the
            // pointer's side otherwise -- the head turns, the body stays.
            const dx = pointer.point.position.x - root.headX
            return Math.max(-40, Math.min(40, dx * 0.12 * -root.facing)) + 20 * root.facing
        }
    }
    Binding {
        target: rig
        property: "headPitch"
        value: root.watching
               ? Math.max(-22, Math.min(28, (root.headY - pointer.point.position.y) * 0.1))
               : 0
    }

    // -- The stage: a small 3D viewport standing on the floor ---------------
    Item {
        id: stage
        width: 190
        height: 130
        // The camera below puts the model's floor (y = 0) at 89 px down the
        // viewport, so this rests the paws on the bottom edge.
        x: root.catX - width / 2 + purr.offset
        y: root.height - 89 - 1 - hop.lift

        View3D {
            id: view
            anchors.fill: parent
            renderMode: View3D.Offscreen
            environment: SceneEnvironment {
                backgroundMode: SceneEnvironment.Transparent
                antialiasingMode: SceneEnvironment.MSAA
                antialiasingQuality: SceneEnvironment.High
                tonemapMode: SceneEnvironment.TonemapModeNone
            }

            PerspectiveCamera {
                position: Qt.vector3d(0, 0.9, 5.2)
                eulerRotation.x: -6
                fieldOfView: 20
                clipNear: 0.5
                clipFar: 20
            }
            DirectionalLight {
                eulerRotation: Qt.vector3d(-55, -25, 0)
                brightness: 1.05
                ambientColor: Qt.rgba(0.42, 0.42, 0.46, 1)
            }
            DirectionalLight {
                eulerRotation: Qt.vector3d(-15, 160, 0)
                brightness: 0.35
            }

            CatRig {
                id: rig
                texture: "textures/cat_" + root.variant + ".png"
                eulerRotation.y: root.action === "sleep" ? root.facingYaw * 0.8 : root.facingYaw
            }
        }

        MouseArea {
            id: catMouse
            anchors.horizontalCenter: parent.horizontalCenter
            y: 30
            width: 120
            height: 62
            enabled: !root.paused
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton
            onClicked: root.pet()
            onDoubleClicked: hop.start()
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Pet the cat")
        }

        // Hearts rise from the cat when petted.
        Repeater {
            id: hearts
            model: 3
            delegate: Image {
                id: heart
                required property int index
                source: "textures/heart.png"
                smooth: false
                width: 12
                height: 12
                x: stage.width / 2 - 6 + (index - 1) * 16
                y: 34
                opacity: 0
                property real rise: 0
                transform: Translate { y: -heart.rise }
                SequentialAnimation {
                    id: riseAnim
                    PauseAnimation { duration: heart.index * 140 }
                    ParallelAnimation {
                        NumberAnimation { target: heart; property: "rise"; from: 0; to: 34; duration: 1100; easing.type: Easing.OutQuad }
                        SequentialAnimation {
                            NumberAnimation { target: heart; property: "opacity"; to: 1; duration: 160 }
                            PauseAnimation { duration: 560 }
                            NumberAnimation { target: heart; property: "opacity"; to: 0; duration: 380 }
                        }
                    }
                }
                // Each pet() bumps root.petPulse; every heart starts its own
                // staggered float from that, no method call across the
                // delegate boundary needed.
                Connections {
                    target: root
                    function onPetPulseChanged() { riseAnim.restart() }
                }
            }
        }

        // Asleep: a small "z" drifts up every few seconds.
        Image {
            id: zee
            source: "textures/sleep_z.png"
            smooth: false
            width: 10
            height: 10
            x: stage.width / 2 + root.facing * 36
            y: 40 - zee.rise
            opacity: 0
            property real rise: 0
            SequentialAnimation {
                id: zeeFloat
                ParallelAnimation {
                    NumberAnimation { target: zee; property: "rise"; from: 0; to: 22; duration: 1800 }
                    SequentialAnimation {
                        NumberAnimation { target: zee; property: "opacity"; to: 0.9; duration: 300 }
                        PauseAnimation { duration: 1000 }
                        NumberAnimation { target: zee; property: "opacity"; to: 0; duration: 500 }
                    }
                }
            }
            Timer {
                interval: 3800
                repeat: true
                running: root.action === "sleep" && !root.paused
                onTriggered: zeeFloat.restart()
            }
        }
    }

    // -- Petting and hopping --------------------------------------------------
    function pet() {
        if (root.action === "walk") {
            stroll.stop()
        }
        if (root.action === "sleep")
            root.setAction("loaf")
        else if (root.action !== "loaf")
            root.setAction("idle")
        root.petPulse += 1
        purr.start()
        brain.interval = 5000
        brain.restart()
    }

    // A purr: the cat shivers by a pixel for a moment.
    QtObject {
        id: purr
        property real offset: 0
        property int ticks: 0
        function start() { ticks = 30; purrTimer.restart() }
    }
    Timer {
        id: purrTimer
        interval: 45
        repeat: true
        onTriggered: {
            purr.ticks -= 1
            purr.offset = purr.ticks > 0 ? (purr.ticks % 2 ? 0.7 : -0.7) : 0
            if (purr.ticks <= 0)
                stop()
        }
    }

    QtObject {
        id: hop
        property real lift: 0
        function start() {
            if (root.action === "sleep" || hopAnim.running)
                return
            stroll.stop()
            root.setAction("idle")
            hopAnim.restart()
        }
    }
    SequentialAnimation {
        id: hopAnim
        NumberAnimation { target: rig; property: "loaf"; to: 0.35; duration: 120 }
        ParallelAnimation {
            NumberAnimation { target: rig; property: "loaf"; to: 0; duration: 140 }
            NumberAnimation { target: hop; property: "lift"; to: 26; duration: 260; easing.type: Easing.OutQuad }
        }
        NumberAnimation { target: hop; property: "lift"; to: 0; duration: 240; easing.type: Easing.InQuad }
        NumberAnimation { target: rig; property: "loaf"; to: 0.25; duration: 80 }
        NumberAnimation { target: rig; property: "loaf"; to: 0; duration: 160 }
    }

    // -- Demo poses -----------------------------------------------------------
    onDemoChanged: applyDemo()
    Component.onCompleted: {
        if (root.demo !== "")
            applyDemo()
        else
            root.setAction("idle")
    }
    function applyDemo() {
        if (root.demo === "")
            return
        stroll.stop()
        brain.stop()
        root.catX = root.width * 0.6
        root.facing = 1
        root.setAction(root.demo === "pet" ? "idle" : root.demo)
        brain.stop()
        if (root.demo === "walk")
            rig.walkPhase = Math.PI / 2
        if (root.demo === "sleep")
            rig.breath = 1
        if (root.demo === "pet")
            Qt.callLater(root.pet)
    }
}
