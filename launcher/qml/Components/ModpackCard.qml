// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One modpack in Discover's results, laid out like a Modrinth App search
 * result: a card of its own with the pack's logo, "Title by author", the
 * pitch over up to two lines and its categories as tags, with the download
 * count and when it was last updated on the right. The whole card opens the
 * pack's page -- nothing here installs directly, since installing means
 * picking a version first (see ModpackDetail.qml). The gallery shot is the
 * pack page's; a list of 16:9 covers showed three packs to a screen.
 *
 * Every card is the same height: one title line, two pitch lines, one tag
 * line, whatever the pack has to say, so the list keeps an even rhythm.
 * Hover lifts it like every other card: one step lighter and a small
 * shadow.
 *
 * `skeleton: true` swaps every field for a shimmering Skeleton block in
 * the same geometry, for the loading cards DiscoverPage shows before a page
 * of results comes back. That is also why every property below has a plain
 * default rather than being `required`: a skeleton card is not bound to a
 * model row at all.
 */
AbstractButton {
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
    // The pack page's cover; carried for it, not shown on the card.
    property string galleryUrl: ""
    // A colour, or undefined/null when the provider has none for this
    // project (CurseForge never does) -- see the models' accentColor role.
    property var accentColor: undefined
    property bool skeleton: false
    // Kept for callers of the old table layout; a card ends itself.
    property bool lastRow: false

    // Modrinth's list result has a 100px icon on a 16px base: two of
    // Kite's large controls, at its 15px one.
    readonly property int logoSize: Theme.control.heightLg * 2
    readonly property int logoSourceSize: logoSize * 2
    readonly property int statsWidth: 168
    // Roomier than a list row: Modrinth's result cards breathe.
    readonly property int cardPadding: Theme.space.lg + Theme.space.xs
    readonly property int maxTags: 4
    // A result's title a step above a card title: the subheading.
    readonly property int titleSize: Theme.type.subheading.pixelSize
    readonly property real titleLine: Theme.type.subheading.lineHeightPx
    readonly property real pitchLine: Theme.type.body.lineHeightPx
    readonly property int pitchLines: 2

    // When the pack last changed, always as an age ("today", "5 days ago",
    // "3 months ago", "2 years ago") after "Updated", so every card in the
    // list says it the same way -- Format.lastPlayed() switches to a
    // calendar date after a month, which put "5 days ago" and "Dec 13, 2025"
    // in one column.
    readonly property string updatedText: {
        var ms = Date.parse(root.updated || "")
        if (!(ms > 0))
            return ""
        var days = Math.floor((Date.now() - ms) / (24 * 60 * 60 * 1000))
        if (days < 1)
            return qsTr("today")
        if (days === 1)
            return qsTr("yesterday")
        if (days < 30)
            return qsTr("%1 days ago").arg(days)
        if (days < 365) {
            var months = Math.max(1, Math.floor(days / 30))
            return months === 1 ? qsTr("1 month ago") : qsTr("%1 months ago").arg(months)
        }
        var years = Math.floor(days / 365)
        return years === 1 ? qsTr("1 year ago") : qsTr("%1 years ago").arg(years)
    }

    function titleCase(word) {
        return word.length > 0 ? word.charAt(0).toUpperCase() + word.slice(1) : word
    }

    // The logo's height, or the words' if they run taller.
    implicitHeight: Math.max(root.logoSize, textColumn.implicitHeight) + root.cardPadding * 2
    padding: root.cardPadding
    hoverEnabled: !root.skeleton
    enabled: !root.skeleton
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description

    background: Rectangle {
        id: card
        radius: Theme.radius.lg
        color: root.down ? Theme.palette.surfaceRaised
             : root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }

        Shadow {
            radius: card.radius
            level: Theme.elevation.sm
            opacity: root.hovered ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        }
    }

    // The hairline (or the focus ring), over the content so the logo's
    // corner matte never paints across it.
    Rectangle {
        anchors.fill: parent
        z: 1
        radius: Theme.radius.lg
        color: "transparent"
        border.width: root.visualFocus ? Theme.focus.width : 1
        border.color: root.visualFocus ? Theme.palette.focusRing
                    : root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // Sets the tag row's height without depending on Tag's own sizing.
    Tag {
        id: tagProbe
        visible: false
        text: "M"
    }

    contentItem: RowLayout {
        spacing: Theme.space.lg + Theme.space.xs

        // -- Logo ------------------------------------------------------------
        Item {
            Layout.preferredWidth: root.logoSize
            Layout.preferredHeight: root.logoSize
            Layout.alignment: Qt.AlignTop

            // No tile of its own behind the logo: it sits on the card.
            Skeleton {
                anchors.fill: parent
                radius: Theme.radius.lg
                visible: root.skeleton
            }
            Image {
                id: logoImg
                anchors.fill: parent
                visible: !root.skeleton && opacity > 0
                source: root.skeleton ? "" : root.logoUrl
                sourceSize: Qt.size(root.logoSourceSize, root.logoSourceSize)
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
                size: Theme.icon.lg
                color: Theme.palette.textTertiary
            }
            // Rounds the logo in the card's own colour.
            CornerMatte {
                visible: logoImg.visible
                cornerRadius: Theme.radius.lg
                matte: card.color
            }
        }

        // -- Title, pitch, categories ----------------------------------------
        ColumnLayout {
            id: textColumn
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space.xxs

            // "Fabulously Optimized by robotkoer", the way Modrinth names a
            // result: the title keeps its room, the author gives way first.
            // Placed by hand: a layout sizing the title from its own width
            // loops.
            Item {
                Layout.fillWidth: true
                implicitHeight: root.titleLine

                Text {
                    id: titleText
                    width: Math.min(implicitWidth, parent.width)
                    height: root.titleLine
                    visible: !root.skeleton
                    verticalAlignment: Text.AlignVCenter
                    text: root.title
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.heading
                    font.pixelSize: root.titleSize
                    font.weight: Theme.type.subheading.weight

                    HoverHandler { id: titleHover; enabled: titleText.truncated }
                    ToolTip.visible: titleText.truncated && titleHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: root.title
                }
                Text {
                    x: titleText.width + Theme.space.xs
                    width: Math.max(0, parent.width - x)
                    anchors.baseline: titleText.baseline
                    visible: !root.skeleton && root.author.length > 0 && width > 0
                    text: qsTr("by %1").arg(root.author)
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Skeleton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.4
                    height: root.titleSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: root.pitchLine * root.pitchLines

                Text {
                    anchors.fill: parent
                    visible: !root.skeleton
                    text: root.description
                    wrapMode: Text.Wrap
                    maximumLineCount: root.pitchLines
                    elide: Text.ElideRight
                    // Exactly the reserved line, so two lines always fit.
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.pitchLine
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
                Skeleton {
                    y: (root.pitchLine - height) / 2
                    width: parent.width * 0.7
                    height: Theme.type.body.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }

            // Always reserved, so a pack without categories keeps the card
            // height. Clipped at the column's edge rather than measured: a
            // tag that does not fit is simply not drawn.
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xxs
                implicitHeight: tagProbe.implicitHeight
                clip: true

                Row {
                    spacing: Theme.space.xs
                    visible: !root.skeleton
                    Repeater {
                        model: (root.categories || []).slice(0, root.maxTags)
                        delegate: Tag {
                            required property var modelData
                            text: root.titleCase(String(modelData))
                        }
                    }
                }
            }
        }

        // -- Downloads and last update ---------------------------------------
        ColumnLayout {
            Layout.preferredWidth: root.statsWidth
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space.xs

            Row {
                Layout.alignment: Qt.AlignRight
                // Not every provider counts downloads (ATLauncher).
                visible: !root.skeleton && (root.downloads || 0) > 0
                spacing: Theme.space.xs
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "download"
                    size: Theme.icon.sm
                    color: Theme.palette.textSecondary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 downloads").arg(Format.compactNumber(root.downloads))
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    font.weight: Font.DemiBold
                }
            }
            Row {
                Layout.alignment: Qt.AlignRight
                visible: !root.skeleton && root.updatedText.length > 0
                spacing: Theme.space.xs
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "clock"
                    size: Theme.icon.sm
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Updated %1").arg(root.updatedText)
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
            Skeleton {
                Layout.alignment: Qt.AlignRight
                visible: root.skeleton
                width: root.statsWidth / 2
                height: Theme.type.label.pixelSize
                radius: Theme.radius.sm
            }
        }
    }
}
