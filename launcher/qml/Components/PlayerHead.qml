// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * An account's Minecraft head: the front of the skin's head with its hat layer,
 * from AccountFaceProvider (image://accountface/<accountId>), which draws an
 * account without a skin in the default Steve or Alex. Pixel art: the provider
 * scales the 8x8 face with nearest-neighbour to exactly `sourceSize`, so keep
 * `size` a multiple of 8. While there is no face yet, a quiet square holds its
 * place; never a letter.
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
