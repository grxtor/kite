// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One instance in the library grid: a cover (its pack's own logo, its
 * newest screenshot, or a rendered scene -- see CoverArt.qml) with the
 * instance's icon chip on it,
 * the name, and what it runs. Used directly as a delegate -- the
 * required properties are filled from InstanceList's named roles, so this
 * file never mentions role numbers.
 *
 * Play lives on the cover and appears on hover (always while running, as
 * Stop); click selects, double-click plays, right-click or the "more"
 * button asks the page for the instance menu.
 *
 * While a launch runs, the launch takes Play's place, as on the play bar
 * (PlayDock): the cover dims, its corner chip says "Starting…", a bar runs
 * beside the icon chip where Play would be, and the line under the name
 * gives the stage ("Downloading assets") and how far it is.
 */
Item {
    id: root

    required property string instanceId
    required property string name
    required property string iconKey
    required property bool isRunning
    required property bool canLaunch
    required property var lastLaunch
    required property string gameVersion
    required property string loader
    required property color iconTint
    // Newest screenshot url (InstanceList's coverImage role), or "" when
    // the instance has none yet -- the cover's own art, see CoverArt.qml.
    required property string coverImage
    // A platform pack's own logo (InstanceList's packArt role), shown ahead
    // of any screenshot; "" for every other instance. Not required, so
    // a static review page need not pass it -- InstanceSection forwards it.
    property string packArt: ""
    // Empty while nothing is being launched; progress is 0..1, or negative
    // while the launch cannot say how far along it is.
    required property string launchStatus
    required property real launchProgress

    property bool selected: false
    // Lets a static review page (Gallery.qml) show the hover state.
    property bool forceHovered: false

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal stopRequested()
    // @p anchor: the ⋯ button when that asked, so the menu opens under it;
    // null for a right-click or the Menu key, which open at the pointer.
    signal menuRequested(Item anchor)

    readonly property bool hovered: forceHovered || hoverHandler.hovered
    readonly property bool launching: launchStatus.length > 0
    // One frame width for everything: the cover's inset from the card edge
    // and the text's, so the name lines up with the picture above it.
    readonly property int inset: Theme.space.sm
    // The scenes' own "card" crop is 16:9 (PixelArt.qml); a screenshot is
    // cropped to the same shape.
    readonly property real coverRatio: 9 / 16
    readonly property int coverHeight: Math.round((width - inset * 2) * coverRatio)
    readonly property int coverRadius: Theme.radius.sm
    // The icon chip: 32px of icon (the instance icons' native size) on a
    // 4px frame -- the same chip InstanceListRow shows.
    readonly property int badgeSize: 40
    readonly property int badgeInset: Theme.space.xs
    readonly property int runningDotSize: 7
    // A key the instanceicon provider resolves to its generic icon, so the
    // chip is never empty.
    readonly property string iconSource: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")

    implicitWidth: 240
    implicitHeight: inset + coverHeight + Theme.space.sm + Theme.type.bodyStrong.lineHeightPx
                    + Theme.space.xxs + Theme.type.caption.lineHeightPx + Theme.space.md

    activeFocusOnTab: true
    Keys.onReturnPressed: root.playRequested()
    Keys.onSpacePressed: root.clicked()
    Keys.onMenuPressed: root.menuRequested(null)

    Accessible.role: Accessible.ListItem
    Accessible.name: root.name
    Accessible.description: Format.versionLine(root.loader, root.gameVersion)

    HoverHandler { id: hoverHandler }

    Rectangle {
        id: card
        width: parent.width
        height: parent.height
        radius: Theme.radius.lg
        color: Theme.palette.surface
        // Hover changes this one property -- no fill change, no zoom, no lift.
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : root.hovered ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // ReleaseWithinBounds grabs on press, so the page's "click empty
        // space to deselect" handler underneath never sees a card click.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked() }
            onDoubleTapped: root.playRequested()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            gesturePolicy: TapHandler.ReleaseWithinBounds
            onTapped: { root.forceActiveFocus(); root.clicked(); root.menuRequested(null) }
        }

        Item {
            id: cover
            x: root.inset
            y: root.inset
            width: parent.width - root.inset * 2
            height: root.coverHeight

            CoverArt {
                id: art
                anchors.fill: parent
                radius: root.coverRadius
                source: root.coverImage
                packArt: root.packArt
                tint: root.iconTint
                // No iconKey: the card draws its own icon chip below, the
                // same one whether the cover is a screenshot or a scene.
                seed: root.instanceId
                // Protects the running chip/more button up top and the
                // play button/icon chip down below from a bright photo.
                scrim: "bottom"
                matte: card.color
            }

            // While launching the cover dims and a bar runs along its foot.
            Rectangle {
                anchors.fill: parent
                radius: root.coverRadius
                color: Qt.rgba(0, 0, 0, 0.45)
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
            }

            // Beside the icon chip, in the row Play takes otherwise, so
            // nothing sits on top of it.
            LaunchProgressBar {
                visible: root.launching
                anchors.left: iconBadge.right
                anchors.right: parent.right
                anchors.leftMargin: Theme.space.sm
                anchors.rightMargin: Theme.space.sm
                anchors.verticalCenter: iconBadge.verticalCenter
                onMedia: true
                progress: root.launchProgress
            }

            // Running: a chip in the corner, readable on any picture --
            // Tag's on-media look, with a status dot. "Starting…" until the
            // game is actually up (an instance counts as running from the
            // moment its launch begins).
            Rectangle {
                visible: root.isRunning || root.launching
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Theme.space.sm
                radius: Theme.radius.sm
                height: Theme.control.heightSm - Theme.space.xs
                width: runningRow.implicitWidth + Theme.space.sm * 2
                color: Theme.media.chip
                border.width: 1
                border.color: Theme.media.chipBorder

                Row {
                    id: runningRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        // A perfect circle, like Switch/Slider's own round
                        // parts: computed half-width, not a radius token.
                        width: root.runningDotSize; height: width; radius: width / 2
                        color: root.launching ? Theme.palette.warning : Theme.palette.success
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.launching ? qsTr("Starting…") : qsTr("Running")
                        color: Theme.media.text
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }

            // On the picture, so on the running chip's dark glass: a bare
            // icon in the theme's text colour vanishes on a bright cover.
            IconButton {
                id: moreButton
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space.sm
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                icon.color: Theme.media.text
                opacity: root.hovered ? 1 : 0
                visible: opacity > 0
                focusPolicy: Qt.NoFocus
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                onClicked: root.menuRequested(moreButton)

                // Hover lights the rim, the one thing that changes.
                background: Rectangle {
                    radius: Theme.radius.sm
                    color: Theme.media.chip
                    border.width: 1
                    border.color: moreButton.hovered ? Theme.media.textSecondary : Theme.media.chipBorder
                    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
            }

            // The instance icon, on every card in the same spot and on the
            // same chip -- screenshot or fallback scene, custom icon or the
            // generic one. Not on a pack's own art: that cover is the logo
            // already. (Kept in the layout either way; the launch bar is
            // anchored to it.)
            Rectangle {
                id: iconBadge
                opacity: art.hasPackArt ? 0 : 1
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space.sm
                width: root.badgeSize
                height: root.badgeSize
                radius: Theme.radius.sm
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                Image {
                    anchors.fill: parent
                    anchors.margins: root.badgeInset
                    source: root.iconSource
                    sourceSize: Qt.size(width, height)
                    fillMode: Image.PreserveAspectFit
                    // Rendered artwork: filtered and mipmapped, so any
                    // scaling the scene graph does stays clean.
                    smooth: true
                    mipmap: true
                }
            }

            PlayButton {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space.sm
                size: root.badgeSize
                running: root.isRunning
                enabled: root.isRunning || root.canLaunch
                // A plain fade, no overshoot scale-in (design-plan.md §4/§2.4).
                opacity: !root.launching && (root.hovered || root.isRunning) ? 1 : 0
                visible: opacity > 0
                focusPolicy: Qt.NoFocus
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                onClicked: root.isRunning ? root.stopRequested() : root.playRequested()
            }
        }

        Text {
            id: title
            anchors.top: cover.bottom
            anchors.topMargin: Theme.space.sm
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: root.inset
            anchors.rightMargin: root.inset
            text: root.name
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.bodyStrong.pixelSize
            font.weight: Theme.type.bodyStrong.weight
        }

        // What it runs on the left, when it was last played on the right.
        // The version always gets its full width; the date is the part
        // that gives way -- dropped outright rather than squeezing the
        // version down to "NeoForge 1.2…" (it is still in the list view
        // and on the instance page).
        Item {
            id: meta
            anchors.top: title.bottom
            anchors.topMargin: Theme.space.xxs
            anchors.left: title.left
            anchors.right: title.right
            height: Theme.type.caption.lineHeightPx

            Text {
                id: versionText
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width)
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: root.launching ? Theme.palette.accentText : Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Font.Medium
            }

            Text {
                id: timeText
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0 && versionText.implicitWidth + Theme.space.sm + implicitWidth <= parent.width
                text: root.launching ? (root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : "")
                                     : Format.lastPlayed(root.lastLaunch)
                color: root.launching ? Theme.palette.accentText : Theme.palette.textTertiary
                // Digits of one width, so the number does not jitter.
                font.features: ({ "tnum": 1 })
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // selected border: 4px out from a 12px corner is a 16px corner.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: Theme.radius.xl
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
