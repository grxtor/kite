// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The cat picture chosen in Settings > Appearance (the same packs the classic
 * window's background cat offers), sitting quietly in the bottom right
 * corner of the page area, where the 3D cat walks. A still picture: it never
 * takes a click, never moves and stays well back from the content.
 *
 * Loaded by Main.qml over the page area, only while the TheCat setting is on.
 * Settings turns the 3D cat off when this is turned on, so the two do not
 * share the corner.
 */
Item {
    id: root
    anchors.fill: parent

    readonly property string source: LauncherStore.catPacks
                                     ? LauncherStore.catPacks.sourceFor(SettingsStore.string("BackgroundCat")) : ""
    // The cat is a hint, not a feature of the page: small and faint.
    readonly property int pictureHeight: 150
    readonly property real quiet: 0.45

    // Never in the way of what is underneath.
    enabled: false

    Image {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Theme.space.xl
        height: root.pictureHeight
        width: implicitWidth * height / Math.max(1, implicitHeight)
        source: root.source
        sourceSize.height: root.pictureHeight * 2
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        mipmap: true
        opacity: root.quiet
        visible: status === Image.Ready
    }
}
