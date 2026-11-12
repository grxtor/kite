// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * What the Sahne and Gece heroes say and do, stacked: a "Last played" (or "Selected") eyebrow, the name (stepping
 * down to fit), the line of icons, then Play and ▾ as glass. The looks differ only in where they put this column,
 * what they lay it over, its ink and the glow under Play.
 */
ColumnLayout {
    id: root

    required property Item hero
    property int nameSize: Theme.type.hero.pixelSize
    property color ink: Theme.palette.textPrimary
    property color inkSecondary: Theme.palette.textSecondary
    property color eyebrowInk: Theme.palette.textTertiary

    readonly property real playCenterY: actions.y + actions.height / 2

    readonly property real eyebrowTracking: 0.14
    // The eyebrow is 12 px (it was 11); the block below stays where it was.
    readonly property int eyebrowTrim: 1
    readonly property int nameGapAbove: 10
    readonly property int metaGapAbove: 14
    readonly property int actionsGapAbove: 26
    readonly property int actionsHeight: 54
    readonly property int playWidth: 144
    readonly property int metaSpacing: 18

    spacing: 0

    HomeLabel {
        Layout.fillWidth: true
        Layout.preferredHeight: implicitHeight - root.eyebrowTrim
        text: root.hero.isSelection ? qsTr("Selected") : qsTr("Last played")
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
        crashCause: root.hero.crashCause
        modUpdates: root.hero.modUpdates
        onUpdatesRequested: root.hero.openUpdates()
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
