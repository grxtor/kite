// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The hero in the classic look, the way Lunar Client opens: one instance on
 * a card -- its picture on the calm tile (HomeHeroTile); its name and what
 * it runs (HomeHeroTitle); label over value, when it was last played, for
 * how long and the world it was last in (HomeHeroFacts); at the card's far
 * edge Play and ▾ (HomeHeroActions). The facts sit between the name and Play
 * on a card wide enough to leave room on both sides, so a wide card has no
 * empty middle; else under the name; on a narrow card they become words on
 * the line under it.
 *
 * A real screenshot of the instance, softened to its colours, is the card's
 * backdrop when it has one (the facts then stay under the name, on its
 * darker side); otherwise the card is the neutral surface -- never a
 * stand-in picture. Clicking the card anywhere else opens the instance
 * page. The instance and every signal belong to the HomeHero it is a view
 * of.
 */
Item {
    id: root

    required property Item hero

    // Lunar's scale: a big tile, a wide Play.
    readonly property int tileSize: Theme.control.heightLg * 2 + Theme.space.xxl
    readonly property int playWidth: 176
    readonly property int padding: Theme.space.xxl
    // On a photo the words take the media colours.
    readonly property color ink: root.hero.hasBackdrop ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property color inkLabel: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textTertiary

    // Where the facts go: beside the name with at least factsGap on either
    // side, else under it, else nowhere (the line under the name says when
    // and how long instead). Measured off natural widths only, never off
    // the layout they change.
    readonly property int gap: Theme.space.xl
    readonly property int factsGap: Theme.space.xxl
    readonly property real innerWidth: card.width - root.padding * 2
    readonly property real besideName: root.innerWidth - root.tileSize - actions.implicitWidth - root.gap * 2
    readonly property bool factsBeside: !root.hero.hasBackdrop
                                        && root.besideName - title.naturalWidth >= factsBelow.naturalWidth + root.factsGap * 2
    readonly property bool factsUnder: !root.factsBeside && root.besideName >= factsBelow.naturalWidth
    readonly property bool showsFacts: root.factsBeside || root.factsUnder

    // The page's top padding is part of the hero, so the page can lay the
    // rows under it without knowing the look.
    readonly property int topInset: Theme.space.xl
    readonly property int gapBelow: Theme.space.xxl
    readonly property real playCenterY: root.topInset + card.height / 2

    implicitHeight: root.topInset + root.tileSize + root.padding * 2

    Rectangle {
        id: card
        x: root.hero.columnX
        y: root.topInset
        width: root.hero.columnWidth
        height: root.implicitHeight - root.topInset
        radius: Theme.radius.xl
        color: Theme.palette.surface
        border.width: root.hero.hasBackdrop ? 0 : 1
        border.color: Theme.palette.border

        // Anywhere but its buttons: the instance page.
        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: root.hero.openRequested(root.hero.instanceId)
        }

        // The instance's own newest screenshot, softened to its colours and
        // darker on the words' side.
        CoverArt {
            anchors.fill: parent
            visible: root.hero.hasBackdrop
            source: root.hero.coverImage
            photoSoftness: root.hero.backdropSoftness
            glyph: ""
            showLogo: false
            radius: card.radius
            scrim: "horizontal"
            matte: Theme.palette.canvas
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: root.padding
            spacing: root.gap

            HomeHeroTile {
                Layout.preferredWidth: root.tileSize
                Layout.preferredHeight: root.tileSize
                Layout.alignment: Qt.AlignVCenter
                iconKey: root.hero.hasInstance ? (root.hero.iconKey.length > 0 ? root.hero.iconKey : "default") : ""
                packArt: root.hero.packArt
                matte: card.color
            }

            ColumnLayout {
                // Beside the facts it is only as wide as the name.
                Layout.fillWidth: !root.factsBeside
                Layout.preferredWidth: root.factsBeside ? title.naturalWidth : -1
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                HomeHeroTitle {
                    id: title
                    Layout.fillWidth: true
                    name: root.hero.name
                    versionText: Format.versionLine(root.hero.loader, root.hero.gameVersion)
                    hasCrashed: root.hero.hasCrashed
                    lastLaunch: root.hero.lastLaunch
                    playTimeText: root.hero.playTimeText
                    showsWhen: !root.showsFacts
                    launchStatus: root.hero.launchStatus
                    launchProgress: root.hero.launchProgress
                    onMedia: root.hero.hasBackdrop
                    ink: root.ink
                    inkSecondary: root.inkSecondary
                    progressMaxWidth: root.playWidth * 2
                }

                HomeHeroFacts {
                    id: factsBelow
                    Layout.topMargin: Theme.space.md
                    visible: root.factsUnder && !root.hero.launching
                    instanceId: root.hero.instanceId
                    lastLaunch: root.hero.lastLaunch
                    playTimeText: root.hero.playTimeText
                    worldsModel: root.hero.worldsModel
                    ink: root.ink
                    inkLabel: root.inkLabel
                    onWorldRequested: (id) => root.hero.worldsRequested(id)
                }
            }

            // The facts beside the name, in the middle of the room between
            // it and Play.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.factsBeside

                HomeHeroFacts {
                    anchors.centerIn: parent
                    instanceId: root.hero.instanceId
                    lastLaunch: root.hero.lastLaunch
                    playTimeText: root.hero.playTimeText
                    worldsModel: root.hero.worldsModel
                    ink: root.ink
                    inkLabel: root.inkLabel
                    onWorldRequested: (id) => root.hero.worldsRequested(id)
                }
            }

            // The page's one primary button, at the card's far edge.
            HomeHeroActions {
                id: actions
                hero: root.hero
                Layout.alignment: Qt.AlignVCenter
                playWidth: root.playWidth
            }
        }
    }
}
