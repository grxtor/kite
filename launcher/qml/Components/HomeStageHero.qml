// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The hero of the Sahne look: a full-bleed stage across the top of the
 * page, no card and no frame. Behind it the instance's newest screenshot,
 * softened to its colours, under a scrim that is dark on the words' side
 * and a fade into the page below; without a screenshot just the page with a
 * faint gradient -- never a stand-in picture. Bottom left, the words and
 * Play (HomeHeroLead); on the right a dark glass panel with the worlds
 * played last (HomeStageWorlds), when the stage is wide enough to hold both.
 *
 * Its bottom fade runs into the next section, which starts `overlap` px up
 * inside it. A view of the HomeHero it belongs to.
 */
Item {
    id: root

    required property Item hero

    readonly property int stageHeight: 388
    readonly property int fadeHeight: 150
    // The next section starts this far up into the fade.
    readonly property int overlap: 30
    // Lead's distance from the stage's foot.
    readonly property int leadBottom: 78
    readonly property int worldsInset: 28
    readonly property int worldsWidth: 292
    // Below this width the worlds panel would crowd the name.
    readonly property int worldsMinWidth: 900
    readonly property bool showsWorlds: root.width >= root.worldsMinWidth && worlds.hasWorlds
    // Softer than the hero card keeps: a stage this big would otherwise read
    // what was written on the screenshot.
    readonly property real softness: 0.012
    readonly property color canvas: Theme.palette.canvas
    readonly property color canvasClear: Qt.rgba(canvas.r, canvas.g, canvas.b, 0)

    readonly property int gapBelow: -root.overlap
    readonly property real playCenterY: lead.y + lead.playCenterY

    implicitHeight: root.stageHeight

    // The page with a faint lift at the top: all a stage without a picture
    // shows.
    Rectangle {
        anchors.fill: parent
        visible: !root.hero.hasBackdrop
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.palette.surface }
            GradientStop { position: 1.0; color: root.canvas }
        }
    }

    // The screenshot, dark on the left where the words are (CoverArt's
    // horizontal scrim).
    CoverArt {
        anchors.fill: parent
        visible: root.hero.hasBackdrop
        source: root.hero.coverImage
        photoSoftness: root.softness
        glyph: ""
        showLogo: false
        radius: 0
        scrim: "horizontal"
        matte: root.canvas
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.fadeHeight
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.canvasClear }
            GradientStop { position: 1.0; color: root.canvas }
        }
    }

    // Anywhere but its buttons: the instance page.
    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.hero.openRequested(root.hero.instanceId)
    }

    HomeHeroLead {
        id: lead
        hero: root.hero
        x: root.hero.columnX
        y: root.stageHeight - root.leadBottom - implicitHeight
        width: (root.showsWorlds ? worlds.x - root.worldsInset : root.width - root.hero.columnX) - x
        nameSize: 48
        ink: root.hero.hasBackdrop ? Theme.media.text : Theme.palette.textPrimary
        inkSecondary: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textSecondary
        eyebrowInk: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textTertiary
    }

    HomeStageWorlds {
        id: worlds
        x: root.width - width - root.worldsInset
        y: root.worldsInset
        width: root.worldsWidth
        visible: root.showsWorlds
        worldsModel: root.hero.worldsModel
        onWorldRequested: (id) => root.hero.worldsRequested(id)
    }
}
