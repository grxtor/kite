// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One search result of ContentBrowserView: logo, "Title by author", one line of description (or the install's
 * progress), and "Installed" once it is. Rows share one card, split by hairlines; hover and the picked row share one
 * look, a lifted inset strip with no hairline and no accent.
 */
AbstractButton {
    id: row

    required property var owner
    required property int index
    required property string title
    required property string description
    required property string author
    required property string logoUrl
    required property bool installed

    readonly property bool selected: owner.selectedRow === index
    readonly property string key: owner.projectKey(title, author)
    readonly property var download: owner.downloadFor(key)
    readonly property bool downloading: !!download && download.running
    readonly property bool checking: owner.isChecking(key)
    readonly property bool justInstalled: !!download && download.succeeded
    readonly property bool failed: !!download && download.failed
    property bool lastRow: false

    readonly property int logoSize: 40
    readonly property int logoSourceSize: logoSize * 2
    readonly property int rowHeight: 64

    implicitHeight: rowHeight
    leftPadding: Theme.space.lg
    rightPadding: Theme.space.md
    hoverEnabled: true
    onClicked: owner.select(index, { title: title, author: author, description: description,
                                     logoUrl: logoUrl, installed: installed })

    background: Item {
        // Opaque at rest too, so the logo's corner matte can always paint in exactly this colour.
        StateFill {
            id: strip
            anchors.fill: parent
            anchors.leftMargin: Theme.space.xs
            anchors.rightMargin: Theme.space.xs
            anchors.topMargin: Theme.space.xxs
            anchors.bottomMargin: Theme.space.xxs
            radius: Theme.radius.sm
            // Picked keeps the hover look.
            card: true
            hovered: row.hovered
            active: row.selected
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            height: 1
            color: Theme.palette.divider
            visible: !row.lastRow
        }
        LaunchProgressBar {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.md
            anchors.bottomMargin: Theme.space.xs
            height: Theme.space.xxs
            visible: row.downloading || row.checking
            progress: row.downloading ? row.download.progress : -1
        }
    }

    contentItem: RowLayout {
        spacing: Theme.space.md

        Item {
            Layout.preferredWidth: row.logoSize
            Layout.preferredHeight: row.logoSize

            Image {
                anchors.fill: parent
                source: row.logoUrl
                sourceSize: Qt.size(row.logoSourceSize, row.logoSourceSize)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
            }
            CornerMatte {
                cornerRadius: Theme.radius.sm
                matte: strip.color
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs

            // "Sodium by jellysquid3": the title keeps its room, the author gives way first.
            // Placed by hand: a layout sizing the title from its own width looped.
            Item {
                Layout.fillWidth: true
                implicitHeight: titleText.implicitHeight
                Copy {
                    id: titleText
                    width: Math.min(implicitWidth, parent.width)
                    text: row.title
                    elide: Text.ElideRight
                    role: "bodyStrong"
                }
                Copy {
                    x: titleText.width + Theme.space.xs
                    width: Math.max(0, parent.width - x)
                    anchors.baseline: titleText.baseline
                    visible: row.author.length > 0 && width > 0
                    text: qsTr("by %1").arg(row.author)
                    elide: Text.ElideRight
                    role: "label"
                    tone: "tertiary"
                }
            }
            Copy {
                Layout.fillWidth: true
                text: row.downloading ? (row.download.status || qsTr("Installing…"))
                    : row.checking ? qsTr("Checking what it needs…") : row.description
                elide: Text.ElideRight
                role: "caption"
                tone: row.downloading || row.checking ? "secondary" : "tertiary"
            }
        }

        StatusBadge {
            visible: row.installed || row.justInstalled || row.failed
            tone: row.failed ? "danger" : "success"
            text: row.failed ? qsTr("Failed") : qsTr("Installed")
        }
    }
}
