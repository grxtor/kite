// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The cat picture chosen in Settings > Appearance (the same packs the classic
 * window's background cat offers), sitting quietly in the bottom right
 * corner of the page area. A still picture: it never takes a click, never
 * moves and stays well back from the content.
 *
 * Loaded by Main.qml under the pages, only while the TheCat setting is on:
 * a page's own cards and lists cover it, so it shows in the empty space.
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
