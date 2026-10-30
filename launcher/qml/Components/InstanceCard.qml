// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * One instance in the library grid, the way Modrinth App's library shows
 * one: a large square, a small inset from the card's edge, with the
 * instance's icon all but filling a calm neutral tile (InstanceTile),
 * then the name and what it runs. A pack installed from a platform shows
 * its own logo across the whole square instead. Most instances have no
 * screenshot, so the card is built around the icon, never around an empty
 * cover.
 *
 * Nothing here is ever a stand-in picture, and hovering changes the card,
 * never what is in it: no screenshot swaps in, no chip appears on the art.
 *
 * Used directly as a delegate -- the required properties are filled from
 * InstanceList's named roles, so this file never mentions role numbers.
 *
 * Play sits in the square's bottom-right corner and fades in on hover
 * (always, as Stop, while running); click selects, double-click plays,
 * right-click or ⋯ -- a bare glyph beside the name -- asks the page for the
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
    // Newest screenshot url (InstanceList's coverImage role); unused here,
    // required so the card takes the same roles as Continue playing's.
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
    // The pack's logo, once it has really loaded; until then (or if it
    // cannot load) the icon stays on the plain surface.
    readonly property bool showsPackArt: root.packArt.length > 0 && packLogo.status === Image.Ready
    // The square sits a small inset from the card's edge, as Modrinth's
    // picture does; the text sits a little further in, under its edge.
    readonly property int inset: Theme.space.sm
    readonly property int textInset: Theme.space.md
    readonly property int squareSize: Math.max(0, width - inset * 2)
    // The icon all but fills the square (Modrinth's picture fills its own):
    // the card's picture, with only a rim of the calm tile around it.
    readonly property real iconShare: 0.82
    readonly property int iconSize: Math.round(squareSize * iconShare)
    // Its own icon, or the built-in one it was given (Icons.displayKey).
    readonly property string drawnKey: Icons.displayKey(root.iconKey, root.instanceId)
    readonly property bool iconFills: Icons.fillsTile(root.drawnKey)
    readonly property int squareRadius: Theme.radius.md
    // Play and ⋯ on the square, and their distance from its corners.
    readonly property int playSize: Theme.control.height
    readonly property int cornerInset: Theme.space.sm
    readonly property int runningDotSize: 7
    // A key the instanceicon provider resolves to its generic icon, so the
    // square is never empty.
    readonly property string iconSource: Icons.instanceIcon(root.drawnKey)

    implicitWidth: 180
    implicitHeight: inset + squareSize + Theme.space.sm + Theme.type.title.lineHeightPx
                    + Theme.type.label.lineHeightPx + textInset

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
        color: root.hovered || root.selected ? Theme.palette.surfaceHover : Theme.palette.surface

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

        // The instance's tile, raised while the card is hovered; a picture
        // across it gets its corners in the card's own colour.
        InstanceTile {
            id: square
            x: root.inset
            y: root.inset
            width: root.squareSize
            height: root.squareSize
            radius: root.squareRadius
            matte: root.showsPackArt || root.iconFills ? card.color : "transparent"
            lifted: root.hovered

            // The instance's icon, centred, or across the tile when it is an
            // opaque square. The provider renders it at the requested size,
            // so it stays sharp at any card width.
            Image {
                anchors.centerIn: parent
                visible: !root.showsPackArt
                width: root.iconFills ? parent.width : root.iconSize
                height: width
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

            // While launching the square dims and a bar runs along its foot.
            KiteShape {
                anchors.fill: parent
                radius: root.squareRadius
                color: Theme.media.chip
                opacity: root.launching ? 1 : 0
                visible: opacity > 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
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
            anchors.leftMargin: root.textInset
            anchors.rightMargin: root.textInset
            height: Theme.type.title.lineHeightPx
            verticalAlignment: Text.AlignVCenter
            text: root.name
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Theme.type.title.weight

            HoverHandler { id: titleHover; enabled: title.truncated }
            ToolTip.visible: title.truncated && titleHover.hovered
            ToolTip.delay: 500
            ToolTip.text: root.name
        }

        // What it runs -- loader and version, nothing else, as Modrinth's
        // card says it (when it was last played is in the list view, on
        // Continue playing and on the instance page). While a launch runs: its stage
        // on the left, how far on the right.
        Item {
            id: meta
            anchors.top: title.bottom
            anchors.left: title.left
            anchors.right: title.right
            height: Theme.type.label.lineHeightPx

            Text {
                id: versionText
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                // Short of ⋯ at the line's end (moreButton), shown or not,
                // so nothing reflows under the pointer.
                width: Math.min(implicitWidth, parent.width - moreButton.width)
                text: root.launching ? root.launchStatus : Format.versionLine(root.loader, root.gameVersion)
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: text.length > 0 && versionText.implicitWidth + Theme.space.sm + implicitWidth <= parent.width
                text: root.launching && root.launchProgress >= 0 ? qsTr("%1%").arg(Math.round(root.launchProgress * 100)) : ""
                color: Theme.palette.textSecondary
                // Digits of one width, so the number does not jitter.
                font.features: ({ "tnum": 1 })
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
        }
    }

    // ⋯ at the end of the version line, a bare glyph -- no chip of its own
    // on the card -- shown while the card is hovered. On that line rather
    // than the name's, so a long name keeps the card's whole width.
    IconButton {
        id: moreButton
        // Placed rather than anchored: the line is the card's child, this
        // the card's sibling.
        x: card.width - width - (root.textInset - Theme.space.xs)
        y: meta.y + (meta.height - height) / 2
        size: Theme.control.heightSm
        iconName: "more"
        tip: qsTr("More")
        // Not while a launch runs: its percentage has the line's end then.
        opacity: root.hovered && !root.launching ? 1 : 0
        visible: opacity > 0
        focusPolicy: Qt.NoFocus
        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
        onClicked: root.menuRequested(moreButton)
    }

    // The card's hairline, drawn over everything so the pictures' corner
    // matte never paints across it. Selected keeps the hover look (fill and
    // border), with no accent ring.
    Rectangle {
        anchors.fill: card
        radius: card.radius
        color: "transparent"
        border.width: 1
        border.color: root.selected || root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the
        // selected state: 4px out from the card's corner.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: card.radius + Theme.space.xs
        color: "transparent"
        border.width: Theme.focus.width
        border.color: Theme.palette.focusRing
        visible: root.activeFocus && !root.selected
    }
}
