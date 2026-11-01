// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One modpack in Discover's results; the whole card opens the pack's page
 * (installing needs a version pick first, see ModpackDetail.qml). Every card
 * is the same height. `skeleton: true` swaps every field for a Skeleton
 * block, so the properties below have defaults instead of being `required`.
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
    // A colour, or undefined/null when the provider has none (CurseForge never does).
    property var accentColor: undefined
    property bool skeleton: false

    readonly property int logoSize: Theme.control.heightLg * 2
    readonly property int logoSourceSize: logoSize * 2
    readonly property int statsWidth: 168
    readonly property int cardPadding: Theme.space.lg + Theme.space.xs
    readonly property int maxTags: 4
    readonly property int titleSize: Theme.type.subheading.pixelSize
    readonly property real titleLine: Theme.type.subheading.lineHeightPx
    readonly property real pitchLine: Theme.type.body.lineHeightPx
    readonly property int pitchLines: 2

    // Always an age after "Updated" ("5 days ago"): Format.lastPlayed() switches to a calendar date
    // after a month, which mixed the two in one column.
    readonly property string updatedText: Format.ageText(root.updated)

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

    // Over the content so the logo's corner matte never paints across the hairline.
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

        Item {
            Layout.preferredWidth: root.logoSize
            Layout.preferredHeight: root.logoSize
            Layout.alignment: Qt.AlignTop

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
            CornerMatte {
                visible: logoImg.visible
                cornerRadius: Theme.radius.lg
                matte: card.color
            }
        }

        ColumnLayout {
            id: textColumn
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space.xxs

            // "Title by author": the title keeps its room, the author gives way first. Placed by
            // hand: a layout sizing the title from its own width loops.
            Item {
                Layout.fillWidth: true
                implicitHeight: root.titleLine

                Copy {
                    id: titleText
                    width: Math.min(implicitWidth, parent.width)
                    height: root.titleLine
                    visible: !root.skeleton
                    verticalAlignment: Text.AlignVCenter
                    text: root.title
                    elide: Text.ElideRight
                    role: "subheading"

                    HoverHandler { id: titleHover; enabled: titleText.truncated }
                    ToolTip.visible: titleText.truncated && titleHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: root.title
                }
                Copy {
                    x: titleText.width + Theme.space.xs
                    width: Math.max(0, parent.width - x)
                    anchors.baseline: titleText.baseline
                    visible: !root.skeleton && root.author.length > 0 && width > 0
                    text: qsTr("by %1").arg(root.author)
                    elide: Text.ElideRight
                    role: "label"
                    tone: "secondary"
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

                Copy {
                    anchors.fill: parent
                    visible: !root.skeleton
                    text: root.description
                    wrapMode: Text.Wrap
                    maximumLineCount: root.pitchLines
                    elide: Text.ElideRight
                    lineHeightMode: Text.FixedHeight
                    lineHeight: root.pitchLine
                    tone: "secondary"
                }
                Skeleton {
                    y: (root.pitchLine - height) / 2
                    width: parent.width * 0.7
                    height: Theme.type.body.pixelSize
                    radius: Theme.radius.sm
                    visible: root.skeleton
                }
            }

            // Always reserved, so a pack without categories keeps the card height; a tag that does
            // not fit is not drawn.
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
                            text: Format.titleCase(String(modelData))
                        }
                    }
                }
            }
        }

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
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 downloads").arg(Format.compactNumber(root.downloads))
                    role: "label"
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
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Updated %1").arg(root.updatedText)
                    role: "label"
                    tone: "secondary"
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
