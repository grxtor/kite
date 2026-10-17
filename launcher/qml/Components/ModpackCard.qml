// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One modpack in Discover's results: a row of the results table, the way
 * Modrinth and the CurseForge app list packs -- the logo, "Title by author",
 * the one-line pitch and up to three categories, with the download count
 * and when it was last updated on the right. Rows share one card and are
 * split by hairlines; hover tints an inset strip, as Add content's rows do
 * (ContentResultRow.qml). The whole row opens the pack's detail page --
 * nothing here installs directly, since installing means picking a version
 * first (see ModpackDetail.qml). The gallery shot is the detail page's;
 * a list of 16:9 covers showed three packs to a screen.
 *
 * Every row is the same height: one title line, one pitch line, one tag
 * line, whatever the pack has to say, so the table reads as a table.
 *
 * `skeleton: true` swaps every field for a shimmering Skeleton block in
 * the same geometry, for the loading rows DiscoverPage shows before a page
 * of results comes back. That is also why every property below has a plain
 * default rather than being `required`: a skeleton row is not bound to a
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
    // The detail page's cover; carried for it, not shown in the row.
    property string galleryUrl: ""
    // A colour, or undefined/null when the provider has none for this
    // project (CurseForge never does) -- see the models' accentColor role.
    property var accentColor: undefined
    property bool skeleton: false
    // The last row draws no hairline under itself: the card's border ends it.
    property bool lastRow: false

    readonly property int logoSize: 64
    readonly property int logoSourceSize: logoSize * 2
    readonly property int statsWidth: 148
    readonly property int maxTags: 3
    readonly property real titleLine: Theme.type.title.lineHeightPx
    readonly property real pitchLine: Theme.type.label.lineHeightPx
    readonly property int tagRowHeight: Theme.control.heightSm - Theme.space.xs
    // What a logo shows until its picture arrives: one neutral step off the
    // card's own surface in either theme.
    readonly property color placeholderColor: Theme.dark ? Theme.palette.surfaceRaised
                                                         : Theme.palette.surfaceSunken

    // When the pack last changed, always as an age ("today", "5 days ago",
    // "3 months ago", "2 years ago") after "Updated", so every row in the
    // table says it the same way -- Format.lastPlayed() switches to a
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

    implicitHeight: root.logoSize + Theme.space.md * 2
    leftPadding: Theme.space.md
    rightPadding: Theme.space.lg
    hoverEnabled: !root.skeleton
    enabled: !root.skeleton
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.description

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.xs
            anchors.rightMargin: Theme.space.xs
            anchors.topMargin: Theme.space.xxs
            anchors.bottomMargin: Theme.space.xxs
            radius: Theme.radius.sm
            color: root.down ? Theme.palette.pressedOverlay
                 : root.hovered ? Theme.palette.surfaceRaised : "transparent"
            border.width: root.visualFocus ? 2 : 0
            border.color: Theme.palette.focusRing
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            height: 1
            color: Theme.palette.divider
            visible: !root.lastRow
        }
    }

    contentItem: RowLayout {
        spacing: Theme.space.md

        // -- Logo ------------------------------------------------------------
        Rectangle {
            Layout.preferredWidth: root.logoSize
            Layout.preferredHeight: root.logoSize
            Layout.alignment: Qt.AlignVCenter
            radius: Theme.radius.sm
            color: root.placeholderColor
            clip: true

            Skeleton {
                anchors.fill: parent
                radius: Theme.radius.sm
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
        }

        // -- Title, pitch, categories ----------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignVCenter
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
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold

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
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Skeleton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.4
                    height: Theme.type.title.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: root.pitchLine

                Text {
                    anchors.fill: parent
                    visible: !root.skeleton
                    verticalAlignment: Text.AlignVCenter
                    text: root.description
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Skeleton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width * 0.7
                    height: Theme.type.label.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }

            // Always reserved, so a pack without categories keeps the row
            // height. Clipped at the column's edge rather than measured: a
            // third tag that does not fit is simply not drawn.
            Item {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xxs
                implicitHeight: root.tagRowHeight
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
            Layout.alignment: Qt.AlignVCenter
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
                    color: Theme.palette.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 downloads").arg(Format.compactNumber(root.downloads))
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                    font.weight: Font.Medium
                }
            }
            Text {
                Layout.alignment: Qt.AlignRight
                visible: !root.skeleton && root.updatedText.length > 0
                text: qsTr("Updated %1").arg(root.updatedText)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
            Skeleton {
                Layout.alignment: Qt.AlignRight
                visible: root.skeleton
                width: root.statsWidth / 2
                height: Theme.type.caption.pixelSize
                radius: Theme.radius.sm
            }
        }
    }
}
