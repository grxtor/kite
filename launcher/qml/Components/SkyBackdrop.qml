// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The Gece look's window background: kiteclient.com's night sky
 * (Theme.sky, deepening from the zenith towards the horizon's violet) with
 * square stars along the top bar, the website's own pixel stars. Main.qml lays it under the whole shell while Theme.look is
 * "night"; the shell and the page panel are then transparent over it.
 *
 * The stars come from a fixed seed, so they sit in the same places on every
 * start and every window size scales them along, never reshuffles them.
 */
Item {
    id: root

    // A miniature (LookMiniature) draws its own few instead.
    property int starCount: 18
    // The top bar's height (Main.qml's shellBarHeight): the stars stay in
    // that band, past where a page title or an instance's trail reaches,
    // where nothing is written. Lower down they sat between words and cards
    // and read as specks of dust.
    property int barHeight: 48
    readonly property real starsFrom: 0.55
    // Short of the friends toggle at the bar's right end.
    readonly property real starsTo: 0.94
    readonly property real starMinOpacity: 0.35
    readonly property real starOpacityRange: 0.35
    readonly property int starSize: 2
    // One star in five is a pixel bigger.
    readonly property real bigStarShare: 0.2

    // Position (x a share of the width, y of the band), opacity and size per
    // star, from a Park-Miller seed.
    readonly property var stars: {
        var seed = 7
        var next = () => (seed = (seed * 16807) % 2147483647) / 2147483647
        var list = []
        for (var i = 0; i < root.starCount; ++i)
            list.push({ x: root.starsFrom + next() * (root.starsTo - root.starsFrom), y: next(),
                        opacity: root.starMinOpacity + next() * root.starOpacityRange,
                        size: next() < root.bigStarShare ? root.starSize + 1 : root.starSize })
        return list
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.sky.zenith }
            GradientStop { position: Theme.sky.highAt; color: Theme.sky.high }
            GradientStop { position: Theme.sky.lowAt; color: Theme.sky.low }
            GradientStop { position: 1.0; color: Theme.sky.horizon }
        }
    }

    Repeater {
        model: root.stars
        delegate: Rectangle {
            required property var modelData
            x: Math.round(modelData.x * root.width)
            y: Math.round(modelData.y * (root.barHeight - modelData.size))
            width: modelData.size
            height: modelData.size
            color: "white"
            opacity: modelData.opacity
        }
    }
}
