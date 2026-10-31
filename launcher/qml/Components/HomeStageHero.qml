// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * The hero of the Sahne look: a full-bleed stage across the top of the page. The newest screenshot, softened, sits
 * under a scrim dark on the words' side and a fade into the page; the next section starts `overlap` px up inside
 * that fade. A view of the HomeHero.
 */
Item {
    id: root

    required property Item hero

    readonly property int stageHeight: 388
    readonly property int fadeHeight: 150
    readonly property int overlap: 30
    readonly property int leadBottom: 78
    readonly property int worldsInset: 28
    readonly property int worldsWidth: 292
    // Below this width the worlds panel would crowd the name.
    readonly property int worldsMinWidth: 900
    readonly property bool showsWorlds: root.width >= root.worldsMinWidth && worlds.hasWorlds
    readonly property real softness: 0.012
    readonly property color canvas: Theme.palette.canvas
    readonly property color canvasClear: Qt.rgba(canvas.r, canvas.g, canvas.b, 0)

    readonly property int gapBelow: -root.overlap
    readonly property real playCenterY: lead.y + lead.playCenterY

    implicitHeight: root.stageHeight

    Rectangle {
        anchors.fill: parent
        visible: !root.hero.hasBackdrop
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.palette.surface }
            GradientStop { position: 1.0; color: root.canvas }
        }
    }

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
