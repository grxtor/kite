// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The hero of the Sade look: no card. A 96 px tile in the kite's cut, the name at 36 px with the line of icons under
 * it, and Play with ▾ at the far edge, all on the page, over a faint glow of the instance's cover (the colour of its
 * icon without one). A view of the HomeHero it belongs to.
 */
Item {
    id: root

    required property Item hero

    readonly property int tileSize: 96
    readonly property int topInset: 44
    readonly property int glowHeight: 330
    readonly property real glowOpacity: 0.32
    readonly property real glowFadeFrom: 0.3
    // Decoded this small and stretched back up: a glow of the cover's colours, not a picture.
    readonly property real glowSoftness: 0.04
    readonly property int gap: Theme.space.xl
    readonly property int actionsHeight: 52
    readonly property int playWidth: 128
    readonly property color canvas: Theme.palette.canvas
    readonly property color canvasClear: Qt.rgba(canvas.r, canvas.g, canvas.b, 0)

    readonly property int gapBelow: 52
    readonly property real playCenterY: root.topInset + root.tileSize / 2

    implicitHeight: root.topInset + root.tileSize

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.glowHeight
        visible: root.hero.hasBackdrop || root.hero.hasTint
        clip: true

        CoverArt {
            anchors.fill: parent
            opacity: root.glowOpacity
            source: root.hero.coverImage
            kind: root.hero.coverKind
            tint: root.hero.iconColor
            photoSoftness: root.glowSoftness
            glyph: ""
            showLogo: false
            radius: 0
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: root.glowFadeFrom; color: root.canvasClear }
                GradientStop { position: 1.0; color: root.canvas }
            }
        }
    }

    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.hero.tap()
    }

    RowLayout {
        x: root.hero.columnX
        y: root.topInset
        width: root.hero.columnWidth
        spacing: root.gap

        HomeHeroTile {
            Layout.preferredWidth: root.tileSize
            Layout.preferredHeight: root.tileSize
            Layout.alignment: Qt.AlignVCenter
            iconKey: root.hero.hasInstance ? (root.hero.iconKey.length > 0 ? root.hero.iconKey : "default") : ""
            instanceId: root.hero.instanceId
            packArt: root.hero.packArt
            matte: root.canvas
        }

        HomeHeroTitle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
            spacing: Theme.space.sm
            name: root.hero.name
            nameSize: Theme.type.banner.pixelSize
            nameLineHeight: 1.1
            iconMeta: true
            metaSize: Theme.type.label.pixelSize
            metaSpacing: Theme.space.lg
            versionText: Format.versionLine(root.hero.loader, root.hero.gameVersion)
            hasCrashed: root.hero.hasCrashed
            lastLaunch: root.hero.lastLaunch
            playTimeText: root.hero.playTimeText
            launchStatus: root.hero.launchStatus
            launchProgress: root.hero.launchProgress
            progressMaxWidth: root.playWidth * 2
        }

        HomeHeroActions {
            hero: root.hero
            Layout.alignment: Qt.AlignVCenter
            size: root.actionsHeight
            playWidth: root.playWidth
            glass: true
            glassFill: Theme.media.glass
        }
    }
}
