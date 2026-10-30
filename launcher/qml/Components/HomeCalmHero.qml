// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The hero of the Sade look: no card. A 96 px tile in the kite's cut, the
 * name at 36 px with the line of icons under it, and Play (the calm green)
 * with ▾ at the far edge, all on the page. Behind the top of the page, the
 * instance's newest screenshot as a faint glow that fades into the page; no
 * screenshot, no glow. A view of the HomeHero it belongs to.
 */
Item {
    id: root

    required property Item hero

    readonly property int tileSize: 96
    readonly property int topInset: 44
    readonly property int glowHeight: 330
    // How much of the screenshot shows, and from where it fades out.
    readonly property real glowOpacity: 0.32
    readonly property real glowFadeFrom: 0.3
    readonly property int gap: Theme.space.xl
    readonly property int actionsHeight: 52
    readonly property int playWidth: 128
    readonly property color canvas: Theme.palette.canvas
    readonly property color canvasClear: Qt.rgba(canvas.r, canvas.g, canvas.b, 0)

    readonly property int gapBelow: 52
    readonly property real playCenterY: root.topInset + root.tileSize / 2

    implicitHeight: root.topInset + root.tileSize

    // The glow: the screenshot softened to its colours, then faded out.
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.glowHeight
        visible: root.hero.hasBackdrop
        clip: true

        CoverArt {
            anchors.fill: parent
            opacity: root.glowOpacity
            source: root.hero.coverImage
            photoSoftness: root.hero.backdropSoftness
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

    // Anywhere but its buttons: the instance page.
    TapHandler {
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: root.hero.openRequested(root.hero.instanceId)
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
            glassFill: Qt.rgba(1, 1, 1, 0.07)
        }
    }
}
