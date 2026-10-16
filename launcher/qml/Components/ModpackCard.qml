// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One modpack in Discover's card grid: a cover image (or, when the pack has
 * no gallery shot, one of PixelArt's rendered scenes picked by its project
 * id), the logo overlapping the cover's foot, title/author, a two-line
 * pitch, up to three categories and a downloads/updated footer. The whole
 * card opens the pack's detail page -- nothing on it installs directly,
 * since installing means picking a version first (see ModpackDetail.qml).
 *
 * EQUAL HEIGHTS. Every text slot has a fixed height -- one title line
 * (elided, the full title in a tooltip) plus the author, two lines of pitch,
 * the tag row always reserved -- and every line is a fixed pixel height
 * (Text.FixedHeight), so a long title, a missing author or a CJK pitch
 * (whose fallback font has taller natural lines) never makes one card in a
 * grid row taller than the rest, nor moves its author or pitch.
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
    // The provider's last-modified date, ISO 8601 -- may be empty.
    property string updated: ""
    property var categories: []
    // Cover image URL; empty falls back to a rendered scene.
    property string galleryUrl: ""
    // A colour, or undefined/null when the provider has none for this
    // project (CurseForge never does) -- see the models' accentColor role.
    property var accentColor: undefined
    property bool skeleton: false

    signal clicked()

    readonly property bool hovered: !root.skeleton && hoverHandler.hovered
    readonly property real coverRatio: 9 / 16
    // A grid sets this from its common card width, so a column a pixel or
    // two wider (the one taking the row's leftover pixels) keeps the same
    // cover -- and so the same title, pitch and footer lines -- as its row.
    property int coverHeight: Math.round(root.width * coverRatio)
    readonly property int logoSize: 56
    // The logo's frame, in the card's own colour, around the picture.
    readonly property int logoFrame: Theme.space.xxs
    // How far the logo sinks into the cover; the rest of it hangs below,
    // into the content area -- the "overlapping the cover's edge" look.
    readonly property int logoOverlap: Math.round(root.logoSize * 0.5)
    readonly property var shownCategories: (root.categories || []).slice(0, 3)
    readonly property bool hasGallery: root.galleryUrl.length > 0 && coverImage.status !== Image.Error
    // What a cover or logo shows until its picture arrives: one neutral
    // step off the card's own surface in either theme. surfaceSunken alone
    // is near-black in the dark theme and read as a broken image there.
    readonly property color placeholderColor: Theme.dark ? Theme.palette.surfaceRaised
                                                         : Theme.palette.surfaceSunken

    // The fixed slot heights (see EQUAL HEIGHTS in the file comment).
    readonly property real titleLine: Theme.type.title.lineHeightPx
    readonly property real authorLine: Theme.type.label.lineHeightPx
    readonly property real pitchLine: Theme.type.body.lineHeightPx
    readonly property int tagRowHeight: Theme.control.heightSm - Theme.space.xs

    // The provider's ISO last-modified date as Format.lastPlayed() words it
    // ("3 h ago", "22 days ago", "Sep 15"), or "" when there is none.
    readonly property string updatedText: {
        var ms = Date.parse(root.updated || "")
        return ms > 0 ? Format.lastPlayed(ms) : ""
    }

    function titleCase(word) {
        return word.length > 0 ? word.charAt(0).toUpperCase() + word.slice(1) : word
    }

    // Which of shownCategories actually fit the card's content width, plus
    // how many were left out -- rather than always laying out all three and
    // letting the card's own clip cut the last one off mid-word at the
    // border. Category names vary a lot in length ("Magic" vs.
    // "Optimization"), and the card grid's own column count changes the
    // available width, so this is measured, not guessed at a fixed count.
    //
    // A plain property recomputed on the specific changes below, not a
    // binding straight off computeTagFit(): that function both writes
    // tagMetrics.text and reads the resulting tagMetrics.width, and a
    // binding that reads a value it just wrote itself is exactly what
    // "Binding loop detected" is warning about.
    property var tagFit: ({ shown: [], hiddenCount: 0 })
    onWidthChanged: tagFit = computeTagFit()
    onShownCategoriesChanged: tagFit = computeTagFit()
    Component.onCompleted: tagFit = computeTagFit()
    function computeTagFit() {
        var cats = root.shownCategories.map(titleCase)
        var maxWidth = Math.max(0, root.width - Theme.space.md * 2)
        var spacing = Theme.space.xs
        if (cats.length === 0)
            return { shown: [], hiddenCount: 0 }
        function chipWidth(text) {
            tagMetrics.text = text
            return tagMetrics.width + Theme.space.sm * 2
        }
        var widths = cats.map(chipWidth)
        var total = widths.reduce(function (a, b) { return a + b }, 0) + spacing * (cats.length - 1)
        if (total <= maxWidth)
            return { shown: cats, hiddenCount: 0 }
        var shown = []
        var used = 0
        for (var i = 0; i < cats.length; ++i) {
            var remaining = cats.length - i - 1
            var overflowW = remaining > 0 ? chipWidth("+" + remaining) + spacing : 0
            var withSpacing = shown.length > 0 ? spacing : 0
            if (used + withSpacing + widths[i] + overflowW > maxWidth)
                break
            used += withSpacing + widths[i]
            shown.push(cats[i])
        }
        return { shown: shown, hiddenCount: cats.length - shown.length }
    }

    TextMetrics {
        id: tagMetrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
        font.weight: Font.Medium
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
        color: Theme.palette.surface
        // Hover changes this one property -- no fill change, no zoom, no lift.
        border.width: 1
        border.color: root.hovered ? Theme.palette.borderStrong : Theme.palette.border

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

            // Under whichever picture is on its way: a gallery shot that has
            // not arrived yet (providers' images routinely take several
            // seconds) shows this flat neutral fill, not the fallback scene
            // below -- that is for a pack that has no gallery shot at all,
            // or whose one failed outright.
            Rectangle {
                anchors.fill: parent
                visible: !root.skeleton
                color: root.placeholderColor
            }

            // No gallery shot: one of the launcher's generic rendered modpack
            // covers (PixelArt.packCoverUrl), picked by project id so a pack
            // always gets the same one.
            Image {
                anchors.fill: parent
                visible: !root.skeleton && !root.hasGallery
                source: visible ? PixelArt.packCoverUrl(root.projectId) : ""
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
            }

            Image {
                id: coverImage
                anchors.fill: parent
                source: root.skeleton ? "" : root.galleryUrl
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
                visible: !root.skeleton && root.galleryUrl.length > 0
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
            }

            // Rectangle `clip` is square, so the cover would poke out past
            // the card's rounded top corners. A ring in the page colour
            // covers exactly those two corners: its inner edge is the card's
            // rounded outline, and it runs on below the cover so its lower
            // corners are clipped away.
            Rectangle {
                readonly property int ring: card.radius + Theme.space.xxs
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
            // title/author, which start below the cover's edge. Both are
            // fixed, so every card's header is the same height.
            height: Math.max(root.logoSize, titleColumn.y + titleColumn.height)

            Item {
                id: logoFrame
                width: root.logoSize
                height: root.logoSize
                z: 2

                // The same neutral fill as the cover's, until the logo
                // arrives (and behind the fallback package glyph).
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius.sm
                    color: root.placeholderColor
                }

                Skeleton {
                    anchors.fill: parent
                    anchors.margins: root.logoFrame
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }

                Image {
                    id: logoImg
                    anchors.fill: parent
                    anchors.margins: root.logoFrame
                    visible: !root.skeleton && opacity > 0
                    source: root.skeleton ? "" : root.logoUrl
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
                }
                MeshIcon {
                    anchors.centerIn: parent
                    visible: !root.skeleton && (root.logoUrl.length === 0 || logoImg.status === Image.Error)
                    iconName: "package"
                    size: Theme.icon.md
                    color: Theme.palette.textTertiary
                }

                // The frame, drawn over the picture: Rectangle's clip is
                // square, and this ring in the card's colour is what gives
                // the logo its rounded corners.
                Rectangle {
                    anchors.fill: parent
                    radius: Theme.radius.sm
                    color: "transparent"
                    border.width: root.logoFrame
                    border.color: card.color
                }
            }

            // Below the cover, never over the picture: a bright gallery
            // shot would swallow the title. A fixed block -- one title line
            // and the author in a fixed slot under it -- so every card in a
            // row has the same title-author-pitch rhythm. A two-line title
            // pushed its author down onto the pitch while one-line cards
            // kept a spare line, so the gap above the pitch varied per card.
            Item {
                id: titleColumn
                anchors.left: logoFrame.right
                anchors.leftMargin: Theme.space.sm
                anchors.right: parent.right
                y: root.logoOverlap + Theme.space.xs
                height: root.titleLine + Theme.space.xxs + root.authorLine

                // One line, elided; the whole title is a hover away in the
                // tooltip, and on the detail page the card opens.
                Text {
                    id: titleText
                    width: parent.width
                    height: root.titleLine
                    visible: !root.skeleton
                    text: root.title
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.titleLine
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold

                    HoverHandler { id: titleHover; enabled: titleText.truncated }
                    ToolTip.visible: titleText.truncated && titleHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: root.title
                }
                Skeleton {
                    width: parent.width * 0.62
                    height: root.titleLine
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }

                Text {
                    y: root.titleLine + Theme.space.xxs
                    width: parent.width
                    height: root.authorLine
                    visible: !root.skeleton
                    text: root.author.length > 0 ? qsTr("by %1").arg(root.author) : ""
                    elide: Text.ElideRight
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.authorLine
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Skeleton {
                    y: root.titleLine + Theme.space.xxs
                    width: parent.width * 0.4
                    height: root.authorLine
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

            Item {
                width: parent.width
                height: root.pitchLine * 2

                Text {
                    anchors.fill: parent
                    visible: !root.skeleton
                    text: root.description
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.pitchLine
                    verticalAlignment: Text.AlignTop
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
                Column {
                    width: parent.width
                    spacing: Theme.space.xs
                    visible: root.skeleton
                    Skeleton { width: parent.width; height: Theme.type.body.pixelSize; radius: Theme.radius.sm }
                    Skeleton { width: parent.width * 0.72; height: Theme.type.body.pixelSize; radius: Theme.radius.sm }
                }
            }

            // Always reserved, so a pack without categories keeps the same
            // height as its neighbours.
            Item {
                width: parent.width
                height: root.tagRowHeight

                Row {
                    spacing: Theme.space.xs
                    visible: !root.skeleton
                    Repeater {
                        model: root.tagFit.shown
                        delegate: Tag { text: modelData }
                    }
                    Tag {
                        visible: root.tagFit.hiddenCount > 0
                        text: "+" + root.tagFit.hiddenCount
                    }
                }
                Row {
                    spacing: Theme.space.xs
                    visible: root.skeleton
                    Repeater {
                        model: 3
                        delegate: Skeleton { width: 52; height: root.tagRowHeight; radius: Theme.radius.sm }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Theme.palette.divider
            }

            Item {
                width: parent.width
                height: Theme.icon.sm

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    // Not every provider counts downloads (ATLauncher).
                    visible: !root.skeleton && (root.downloads || 0) > 0
                    spacing: Theme.space.xs
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
                // When the pack last changed, as a clock and Format's own
                // wording -- the same words an instance's "last played" uses.
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.skeleton && root.updatedText.length > 0
                    spacing: Theme.space.xs
                    Accessible.name: qsTr("Updated %1").arg(root.updatedText)
                    MeshIcon {
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: "clock"
                        size: Theme.icon.sm
                        color: Theme.palette.textTertiary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.updatedText
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
                Skeleton { visible: root.skeleton; anchors.left: parent.left; width: 44; height: parent.height; radius: Theme.radius.sm }
                Skeleton { visible: root.skeleton; anchors.right: parent.right; width: 60; height: parent.height; radius: Theme.radius.sm }
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
        // own border: 4px out from a 12px corner is a 16px corner.
        anchors.fill: card
        anchors.margins: -Theme.space.xs
        radius: Theme.radius.xl
        color: "transparent"
        border.width: 2
        border.color: Theme.palette.focusRing
        visible: root.activeFocus
    }
}
