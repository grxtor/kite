// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The hero of the Gece look: the words straight on the night sky -- a
 * lavender "Last played", the name at 46 px, the line of icons, then a
 * white Play and ▾ as glass. No card, no
 * backdrop of its own: the sky (Main.qml) is the backdrop, and a
 * screenshot would fight it. A view of the HomeHero it belongs to.
 */
Item {
    id: root

    required property Item hero

    readonly property int topInset: 52
    readonly property int nameSize: 46
    readonly property int maxWidth: 620
    readonly property color accentInk: Theme.palette.accentText

    readonly property int gapBelow: 76
    readonly property real playCenterY: root.topInset + lead.playCenterY

    implicitHeight: root.topInset + lead.implicitHeight

    // Anywhere but its buttons: the instance page.
    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.hero.openRequested(root.hero.instanceId)
    }

    HomeHeroLead {
        id: lead
        hero: root.hero
        x: root.hero.columnX
        y: root.topInset
        width: Math.min(root.maxWidth, root.hero.columnWidth)
        nameSize: root.nameSize
        eyebrowInk: root.accentInk
    }
}
