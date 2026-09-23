// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * One modpack in Discover's card grid: a cover image (or a tinted gradient
 * with the project's own logo when it has no gallery shot), the logo
 * overlapping the cover's foot, title/author, a two-line pitch, up to
 * three categories and a downloads/updated footer. The whole card opens
 * the pack's detail page -- nothing on it installs directly, since
 * installing means picking a version first (see ModpackDetail.qml).
 *
 * `skeleton: true` swaps every field for a shimmering Skeleton block in
 * the same geometry, for the loading grid DiscoverPage shows before a
 * page of results comes back. That is also why every property below has
 * a plain default rather than being `required`: a skeleton card is not
 * bound to a model row at all.
 */
Item {
    id: root

    property string projectId: ""
    property string title: ""
    property string author: ""
    property string description: ""
    property string logoUrl: ""
    property var downloads: 0
    // Modrinth's "date_modified", ISO 8601 -- may be empty.
    property string updated: ""
    property var categories: []
    // Cover image URL; empty falls back to a tinted gradient.
    property string galleryUrl: ""
    // A colour, or undefined/null when Modrinth has none for this project
    // -- see ModrinthModpackModel's accentColor role.
    property var accentColor: undefined
    property bool skeleton: false

    signal clicked()

    readonly property bool hovered: !root.skeleton && hoverHandler.hovered
    readonly property color tint: root.accentColor ? root.accentColor : Theme.palette.accent
    readonly property int coverHeight: Math.round(root.width * 0.56)
    readonly property int logoSize: 56
    // How far the logo sinks into the cover; the rest of it hangs below,
    // into the content area -- the "overlapping the cover's edge" look.
    readonly property int logoOverlap: Math.round(root.logoSize * 0.5)
    readonly property var shownCategories: (root.categories || []).slice(0, 3)

    function titleCase(word) {
        return word.length > 0 ? word.charAt(0).toUpperCase() + word.slice(1) : word
    }

    // Modrinth's ISO date -> "3 h ago" / "5 d ago", the same coarseness
    // Format.lastPlayed uses for an instance's last launch. Kept local
    // rather than added there: a pack's "date_modified" has no "never"
    // state to special-case, and Format.qml belongs to a different area.
    function timeAgo(iso) {
        if (!iso)
            return ""
        var ms = Date.parse(iso)
        if (isNaN(ms))
            return ""
        var minutes = Math.max(0, Math.floor((Date.now() - ms) / 60000))
        if (minutes < 60)
            return qsTr("Updated just now")
        var hours = Math.floor(minutes / 60)
        if (hours < 24)
            return qsTr("Updated %1 h ago").arg(hours)
        var days = Math.floor(hours / 24)
        if (days < 30)
            return qsTr("Updated %1 d ago").arg(days)
        var months = Math.floor(days / 30)
        if (months < 12)
            return qsTr("Updated %1 mo ago").arg(months)
        return qsTr("Updated %1 y ago").arg(Math.floor(months / 12))
    }

    implicitWidth: 280
    implicitHeight: card.height

    activeFocusOnTab: !root.skeleton
    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description

    HoverHandler { id: hoverHandler; enabled: !root.skeleton }
    TapHandler {
        enabled: !root.skeleton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: { root.forceActiveFocus(); root.clicked() }
    }

    Rectangle {
        id: card
        width: parent.width
        height: bodyColumn.y + bodyColumn.height + Theme.space.md
        radius: Theme.radius.lg
        clip: true
        color: root.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: 1
        border.color: root.hovered ? Theme.palette.borderStrong : Theme.palette.border
        // The lift is on the card, not the root, so the root's own
        // reported size (what the page's Grid lays out against) never
        // wobbles -- same trick InstanceCard uses.
        y: root.hovered ? -3 : 0

        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        // Cover -- edge to edge; see the corner mask at its end for how the
        // top corners get rounded without a per-corner radius (Qt 6.7+).
        Item {
            id: cover
            x: 0
            y: 0
            width: parent.width
            height: root.coverHeight
            clip: true

            Skeleton { anchors.fill: parent; radius: 0; visible: root.skeleton }

            // Fallback backdrop when there is no gallery shot: a gradient
            // cut from the project's own accent colour, the logo blown up
            // and faded behind a crisp copy of itself -- the closest a
            // shader-free build gets to a "blurred glow".
            Rectangle {
                id: fallback
                anchors.fill: parent
                // Stays under the real cover until it is fully loaded (and
                // reappears if it errors), so the crossfade below never
                // exposes bare card colour for a frame.
                visible: !root.skeleton && coverImage.status !== Image.Ready
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Format.shade(root.tint, Theme.dark ? 0.30 : 0.88, 0.9) }
                    GradientStop { position: 1.0; color: Format.shade(root.tint, Theme.dark ? 0.13 : 0.72, 0.85) }
                }

                Image {
                    id: logoGlow
                    anchors.centerIn: parent
                    width: parent.height * 1.4
                    height: width
                    source: root.logoUrl
                    sourceSize: Qt.size(96, 96)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    opacity: 0.20
                    visible: status === Image.Ready
                }
                Image {
                    id: logoCrisp
                    anchors.centerIn: parent
                    width: Math.round(parent.height * 0.46)
                    height: width
                    source: root.logoUrl
                    sourceSize: Qt.size(160, 160)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    visible: status === Image.Ready
                }
                MeshIcon {
                    anchors.centerIn: parent
                    visible: logoGlow.status !== Image.Ready && logoCrisp.status !== Image.Ready
                    iconName: "package"
                    size: Theme.icon.lg
                    color: Theme.palette.textOnAccent
                    opacity: 0.55
                }
            }

            Image {
                id: coverImage
                anchors.fill: parent
                source: root.skeleton ? "" : root.galleryUrl
                sourceSize: Qt.size(640, 360)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: !root.skeleton && root.galleryUrl.length > 0
                opacity: status === Image.Ready ? 1 : 0
                scale: root.hovered ? 1.06 : 1.0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.slow } }
                Behavior on scale { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
            }

            // Scrim + "view" affordance, on hover only.
            Rectangle {
                anchors.fill: parent
                visible: !root.skeleton
                color: Theme.palette.scrim
                opacity: root.hovered ? 0.35 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
            }
            Rectangle {
                visible: !root.skeleton
                anchors.centerIn: parent
                opacity: root.hovered ? 1 : 0
                scale: root.hovered ? 1 : 0.92
                radius: Theme.radius.pill
                color: Theme.palette.surfaceOverlay
                width: viewRow.implicitWidth + Theme.space.lg * 2
                height: Theme.control.height
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Easing.OutBack } }

                Row {
                    id: viewRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "chevron-right"
                        size: Theme.icon.sm
                        color: Theme.palette.textPrimary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("View pack")
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }

            // Rectangle `clip` is square, so the cover would poke out past
            // the card's rounded top corners (more so while it zooms on
            // hover). A ring in the page colour covers exactly those two
            // corners: its inner edge is the card's rounded outline, and it
            // runs on below the cover so its lower corners are clipped away.
            Rectangle {
                readonly property int ring: card.radius + 2
                x: -ring
                y: -ring
                width: parent.width + ring * 2
                height: parent.height + ring * 2 + card.radius * 2
                radius: card.radius + ring
                color: "transparent"
                border.width: ring
                border.color: Theme.palette.canvas
            }
        }

        // Header: the logo, half sunk into the cover, and title/author
        // beside it.
        Item {
            id: header
            anchors.top: cover.bottom
            anchors.topMargin: -root.logoOverlap
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            // Tall enough for whichever reaches lower: the logo, or the
            // title/author, which start below the cover's edge.
            height: Math.max(root.logoSize, titleColumn.y + titleColumn.implicitHeight)

            Rectangle {
                id: logoFrame
                width: root.logoSize
                height: root.logoSize
                radius: Theme.radius.md
                color: Theme.palette.surface
                border.width: 2
                border.color: Theme.palette.surface
                z: 2

                Skeleton { anchors.fill: parent; anchors.margins: 2; radius: Theme.radius.md - 2; visible: root.skeleton }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Theme.radius.md - 2
                    color: Theme.palette.surfaceSunken
                    clip: true
                    visible: !root.skeleton

                    Image {
                        id: logoImg
                        anchors.fill: parent
                        source: root.logoUrl
                        sourceSize: Qt.size(112, 112)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                    }
                    MeshIcon {
                        anchors.centerIn: parent
                        visible: logoImg.status !== Image.Ready
                        iconName: "package"
                        size: Theme.icon.md
                        color: Theme.palette.textTertiary
                    }
                }
            }

            // Below the cover, never over the picture: a bright gallery
            // shot would swallow the title.
            Column {
                id: titleColumn
                anchors.left: logoFrame.right
                anchors.leftMargin: Theme.space.sm
                anchors.right: parent.right
                y: root.logoOverlap + Theme.space.xs
                spacing: Theme.space.xxs

                Text {
                    width: parent.width
                    visible: !root.skeleton
                    text: root.title
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                Skeleton {
                    width: parent.width * 0.62
                    height: Theme.type.title.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }

                Text {
                    width: parent.width
                    visible: !root.skeleton && root.author.length > 0
                    text: qsTr("by %1").arg(root.author)
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Skeleton {
                    width: parent.width * 0.4
                    height: Theme.type.label.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }
        }

        Column {
            id: bodyColumn
            anchors.top: header.bottom
            anchors.topMargin: Theme.space.sm
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            spacing: Theme.space.sm

            Text {
                width: parent.width
                visible: !root.skeleton
                text: root.description
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                lineHeight: 1.3
            }
            Column {
                width: parent.width
                spacing: 3
                visible: root.skeleton
                Skeleton { width: parent.width; height: Theme.type.body.pixelSize; radius: Theme.radius.sm }
                Skeleton { width: parent.width * 0.72; height: Theme.type.body.pixelSize; radius: Theme.radius.sm }
            }

            Row {
                spacing: Theme.space.xs
                visible: !root.skeleton && root.shownCategories.length > 0
                Repeater {
                    model: root.shownCategories
                    delegate: Tag { text: root.titleCase(modelData) }
                }
            }
            Row {
                spacing: Theme.space.xs
                visible: root.skeleton
                Repeater {
                    model: 3
                    delegate: Skeleton { width: 52; height: Theme.control.heightSm - Theme.space.xs; radius: Theme.radius.sm + 2 }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.palette.divider
                visible: !root.skeleton
            }

            Item {
                width: parent.width
                height: Theme.icon.sm
                visible: !root.skeleton

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space.xxs
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "download"
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: Format.compactNumber(root.downloads)
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                        font.weight: Font.Medium
                    }
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.updated.length > 0
                    text: root.timeAgo(root.updated)
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
            Item {
                width: parent.width
                height: Theme.type.caption.pixelSize
                visible: root.skeleton
                Skeleton { anchors.left: parent.left; width: 44; height: parent.height; radius: Theme.radius.sm }
                Skeleton { anchors.right: parent.right; width: 60; height: parent.height; radius: Theme.radius.sm }
            }
        }

        // The card's border again, on top: the edge-to-edge cover paints
        // over the card's own one along the top and sides.
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border.width: card.border.width
            border.color: card.border.color
        }
    }

    Rectangle {
        // Keyboard focus ring, outset so it never competes with the card's
        // own border.
        x: card.x - 3
        y: card.y - 3
        width: card.width + 6
        height: card.height + 6
        radius: card.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
