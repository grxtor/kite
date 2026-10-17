// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * An account's Minecraft head, the way launchers show who is playing: the
 * front of the skin's head with its hat layer, from AccountFaceProvider
 * (image://accountface/<accountId>), which draws an account without a skin
 * of its own in the game's default Steve or Alex.
 *
 * Pixel art, so drawn pixel for pixel: the provider scales the 8x8 face
 * with nearest-neighbour to exactly the size asked for (sourceSize, which
 * Qt multiplies by the screen's pixel ratio), and nothing smooths it after.
 * Keep `size` a multiple of 8 and every texel comes out the same size.
 *
 * While there is no face to show yet (the default skin still downloading,
 * no account), a quiet square of the same size holds its place. Never a
 * letter: a head is the one avatar a Minecraft player recognises.
 */
Item {
    id: root

    // "image://accountface/<accountId>[?rev=N]", or "" for none.
    property string source: ""
    property int size: 32

    implicitWidth: root.size
    implicitHeight: root.size

    Accessible.ignored: true

    // Under the face, so a transparent one (nothing to draw yet) shows it.
    Rectangle {
        anchors.fill: parent
        color: Theme.palette.surfaceSunken
    }

    Image {
        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(root.size, root.size)
        smooth: false
        mipmap: false
        fillMode: Image.Stretch
    }
}
