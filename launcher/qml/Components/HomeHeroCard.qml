// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The hero in the classic look: one instance on a card with its picture, name, facts and Play with ▾. The facts
 * sit between the name and Play on a wide card, under the name on a narrower one, in the line under the name on a
 * narrow one. A view of the HomeHero it belongs to.
 */
Item {
    id: root

    required property Item hero

    readonly property int tileSize: Theme.control.heightLg * 2 + Theme.space.xxl
    readonly property int playWidth: 176
    readonly property int padding: Theme.space.xxl
    readonly property color ink: root.hero.hasBackdrop ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property color inkLabel: root.hero.hasBackdrop ? Theme.media.textSecondary : Theme.palette.textTertiary

    // Where the facts go: beside the name, else under it, else nowhere. Measured off natural widths only, never off
    // the layout they change.
    readonly property int gap: Theme.space.xl
    readonly property int factsGap: Theme.space.xxl
    readonly property real innerWidth: card.width - root.padding * 2
    readonly property real besideName: root.innerWidth - root.tileSize - actions.implicitWidth - root.gap * 2
    readonly property bool factsBeside: !root.hero.hasBackdrop
                                        && root.besideName - title.naturalWidth >= factsBelow.naturalWidth + root.factsGap * 2
    readonly property bool factsUnder: !root.factsBeside && root.besideName >= factsBelow.naturalWidth
    readonly property bool showsFacts: root.factsBeside || root.factsUnder

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

        TapHandler {
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: root.hero.tap()
        }

        // Inside the hairline when there is no photo, so the card keeps its edge.
        CoverArt {
            readonly property int edge: root.hero.hasBackdrop ? 0 : card.border.width
            anchors.fill: parent
            anchors.margins: edge
            visible: root.hero.hasBackdrop || root.hero.hasTint
            source: root.hero.coverImage
            kind: root.hero.coverKind
            tint: root.hero.iconColor
            glyph: ""
            showLogo: false
            radius: card.radius - edge
            scrim: root.hero.hasBackdrop ? "horizontal" : "none"
            matte: root.hero.hasBackdrop ? Theme.palette.canvas : card.color
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

            HomeHeroActions {
                id: actions
                hero: root.hero
                Layout.alignment: Qt.AlignVCenter
                playWidth: root.playWidth
            }
        }
    }
}
