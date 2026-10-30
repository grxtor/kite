// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * What the Sahne and Gece heroes say and do, stacked: a small "Last played"
 * eyebrow, the instance's name at 46-48 px (stepping down to fit), the line
 * of icons under it (what it runs, when, how long), then Play and ▾ as
 * glass. The two looks differ only in where they put this column, what they
 * lay it over, its ink and the glow under Play -- all inputs here. The
 * instance and the signals belong to the HomeHero it belongs to.
 */
ColumnLayout {
    id: root

    required property Item hero
    property int nameSize: 48
    property color ink: Theme.palette.textPrimary
    property color inkSecondary: Theme.palette.textSecondary
    property color eyebrowInk: Theme.palette.textTertiary

    // Where Play's middle is, in this column's own coordinates.
    readonly property real playCenterY: actions.y + actions.height / 2

    readonly property int eyebrowSize: 11
    readonly property real eyebrowTracking: 0.14
    readonly property int nameGapAbove: 10
    readonly property int metaGapAbove: 14
    readonly property int actionsGapAbove: 26
    readonly property int actionsHeight: 54
    readonly property int playWidth: 144
    readonly property int metaSpacing: 18

    spacing: 0

    HomeLabel {
        Layout.fillWidth: true
        text: qsTr("Last played")
        size: root.eyebrowSize
        tracking: root.eyebrowTracking
        color: root.eyebrowInk
    }

    HomeHeroTitle {
        Layout.fillWidth: true
        Layout.topMargin: root.nameGapAbove
        spacing: root.metaGapAbove
        name: root.hero.name
        nameSize: root.nameSize
        nameLineHeight: 1.05
        iconMeta: true
        metaSize: Theme.type.label.pixelSize
        metaSpacing: root.metaSpacing
        versionText: Format.versionLine(root.hero.loader, root.hero.gameVersion)
        hasCrashed: root.hero.hasCrashed
        lastLaunch: root.hero.lastLaunch
        playTimeText: root.hero.playTimeText
        launchStatus: root.hero.launchStatus
        launchProgress: root.hero.launchProgress
        onMedia: root.hero.hasBackdrop
        ink: root.ink
        inkSecondary: root.inkSecondary
        progressMaxWidth: root.playWidth * 2
    }

    HomeHeroActions {
        id: actions
        hero: root.hero
        Layout.topMargin: root.actionsGapAbove
        size: root.actionsHeight
        playWidth: root.playWidth
        glass: true
    }
}
