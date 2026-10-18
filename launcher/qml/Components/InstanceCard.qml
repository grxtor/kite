// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One instance in the library grid, the way Modrinth App's library shows
 * one: a large square with the instance's icon on a calm neutral tile
 * (PlaceholderSurface), then the name and what it runs. A pack
 * installed from a platform shows its own logo across the whole square
 * instead. Most instances have no screenshot, so the card is built around
 * the icon, never around an empty cover.
 *
 * A real screenshot (InstanceList's coverImage role) is a hover preview: it
 * fades in over the square while the pointer is on the card, so the grid
 * at rest stays one calm row of icons. Nothing here is ever a stand-in
 * picture.
 *
 * Used directly as a delegate -- the required properties are filled from
 * InstanceList's named roles, so this file never mentions role numbers.
 *
 * Play sits in the square's bottom-right corner and fades in on hover
 * (always, as Stop, while running); click selects, double-click plays,
 * right-click or the ⋯ button in the top-right corner asks the page for the
 * instance menu.
 *
 * While a launch runs, the launch takes Play's place, as on the play bar
 * (PlayDock): the square dims, its corner chip says "Starting…", a bar runs
 * along its foot, and the line under the name gives the stage
 * ("Downloading assets") and how far it is.
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
    // the instance has none yet -- the square's hover preview.
    required property string coverImage
    // A platform pack's own logo (InstanceList's packArt role), shown across
    // the square; "" for every other instance. Not required, so a static
    // review page need not pass it -- InstanceSection forwards it.
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
    readonly property bool hasPreview: root.coverImage.length > 0
    // The pack's logo, once it has really loaded; until then (or if it
    // cannot load) the icon stays on the plain surface.
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    // One inset for everything: the square's distance from the card edge
    // and the text's, so the name lines up with the square above it.
    readonly property int inset: Theme.space.md
    readonly property int squareSize: Math.max(0, width - inset * 2)
    // The icon fills this share of the square: big enough to be the
    // card's picture, with the calm surface still framing it.
    readonly property real iconShare: 0.5
    readonly property int iconSize: Math.round(squareSize * iconShare)
    readonly property int squareRadius: Theme.radius.md
    // Play and ⋯ on the square, and their distance from its corners.
    readonly property int playSize: 40
    readonly property int cornerInset: Theme.space.sm
    readonly property int runningDotSize: 7
    // A key the instanceicon provider resolves to its generic icon, so the
    // square is never empty.
    readonly property string iconSource: "image://instanceicon/" + (root.iconKey.length > 0 ? root.iconKey : "default")

    implicitWidth: 180
    implicitHeight: inset + squareSize + Theme.space.sm + Theme.type.bodyStrong.lineHeightPx
                    + Theme.type.caption.lineHeightPx + inset

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
        anchors.fill: parent
        radius: Theme.radius.lg
        // Hover lifts the card: one step lighter and a small shadow under
        // it, never a zoom.
        color: root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: card.radius
            level: Theme.elevation.sm
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }

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
            id: square
            x: root.inset
            y: root.inset
            width: root.squareSize
            height: root.squareSize

            PlaceholderSurface {
                anchors.fill: parent
                radius: root.squareRadius
            }

            // The instance's icon, centred. The provider renders it at the
            // requested size, so it stays sharp at any card width.
            Image {
                anchors.centerIn: parent
                visible: !root.showsPackArt
                width: root.iconSize
                height: root.iconSize
                source: root.iconSource
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            // A pack's own logo is square, made to be shown this way.
            Image {
                id: packLogo
                anchors.fill: parent
                visible: root.showsPackArt
                source: root.packArt
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
            }

            // The newest screenshot, only while the pointer is on the card.
            Image {
                id: preview
                anchors.fill: parent
                source: root.hasPreview ? root.coverImage : ""
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
                opacity: root.hovered && !root.launching && status === Image.Ready ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
            }

            // While launching the square dims and a bar runs along its foot.
            Rectangle {
                anchors.fill: parent
                radius: root.squareRadius
                color: Theme.media.chip
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
            }

            // Rounds the pictures' corners in the card's own colour.
            CornerMatte {
                visible: root.showsPackArt || preview.visible
                cornerRadius: root.squareRadius
                matte: card.color
            }

            LaunchProgressBar {
                visible: root.launching
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: root.cornerInset
                onMedia: true
                progress: root.launchProgress
            }

            // Running: a small chip in the corner with a status dot,
            // "Starting…" until the game is actually up (an instance counts
            // as running from the moment its launch begins). An opaque
            // surface chip, so it reads on the plain square and on a
            // screenshot alike.
            Rectangle {
                visible: root.isRunning || root.launching
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: root.cornerInset
                radius: Theme.radius.indicator
                height: runningRow.implicitHeight + Theme.space.xs * 2
                width: runningRow.implicitWidth + Theme.space.sm * 2
                color: Theme.palette.surfaceOverlay
                border.width: 1
                border.color: Theme.palette.border

                Row {
                    id: runningRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        // A perfect circle: computed half-width, not a
                        // radius token.
                        width: root.runningDotSize; height: width; radius: width / 2
                        color: root.launching ? Theme.palette.warning : Theme.palette.success
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.launching ? qsTr("Starting…") : qsTr("Running")
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }

            IconButton {
                id: moreButton
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: root.cornerInset
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                opacity: root.hovered ? 1 : 0
                visible: opacity > 0
                focusPolicy: Qt.NoFocus
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                onClicked: root.menuRequested(moreButton)

                // An opaque chip, like the running one: it sits on the
                // plain square or on a screenshot.
                background: Rectangle {
                    radius: Theme.radius.sm
                    color: moreButton.hovered ? Theme.palette.surfaceHover : Theme.palette.surfaceOverlay
                    border.width: 1
                    border.color: Theme.palette.border
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }
            }

            PlayButton {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: root.cornerInset
                size: root.playSize
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
            anchors.top: square.bottom
            anchors.topMargin: Theme.space.sm
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: root.inset
            anchors.rightMargin: root.inset
            height: Theme.type.bodyStrong.lineHeightPx
            verticalAlignment: Text.AlignVCenter
            text: root.name
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.bodyStrong.pixelSize
            font.weight: Theme.type.bodyStrong.weight

            HoverHandler { id: titleHover; enabled: title.truncated }
            ToolTip.visible: title.truncated && titleHover.hovered
            ToolTip.delay: 500
            ToolTip.text: root.name
        }

        // What it runs on the left, when it was last played on the right.
        // The version always gets its full width; the date is the part
        // that gives way -- dropped outright rather than squeezing the
        // version down to "NeoForge 1.2…" (it is still in the list view
        // and on the instance page).
        Item {
            id: meta
            anchors.top: title.bottom
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
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
                font.weight: Font.Medium
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0 && versionText.implicitWidth + Theme.space.sm + implicitWidth <= parent.width
                text: root.launching ? (root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : "")
                                     : Format.lastPlayed(root.lastLaunch)
                color: root.launching ? Theme.palette.textSecondary : Theme.palette.textTertiary
                // Digits of one width, so the number does not jitter.
                font.features: ({ "tnum": 1 })
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }
    }

    // The card's hairline, drawn over everything so the pictures' corner
    // matte never paints across it. Selected: a 2px accent ring.
    Rectangle {
        anchors.fill: card
        radius: card.radius
        color: "transparent"
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.palette.accent
                    : root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // selected border: 4px out from the card's corner.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: card.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
