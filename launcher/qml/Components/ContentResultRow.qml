// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One search result of ContentBrowserView, a row of its results table:
 * logo, "Title by author", one line of description (or the install's
 * progress), and "Installed" once it is. Rows share one card and are split
 * by hairlines; hover and the picked row tint an inset strip, the way
 * PackRow's do on the Content tab.
 */
AbstractButton {
    id: row

    // The ContentBrowserView: selection, downloads, select().
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
    // The last row draws no hairline under itself: the card's border ends it.
    property bool lastRow: false

    readonly property int logoSize: 40
    // Pixels of the logo image asked for: twice its size, for HiDPI.
    readonly property int logoSourceSize: logoSize * 2
    readonly property int rowHeight: 60

    implicitHeight: rowHeight
    // The logo sits as far in as on Content's rows.
    leftPadding: Theme.space.md
    rightPadding: Theme.space.md
    hoverEnabled: true
    onClicked: owner.select(index, { title: title, author: author, description: description,
                                     logoUrl: logoUrl, installed: installed })

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.xs
            anchors.rightMargin: Theme.space.xs
            anchors.topMargin: Theme.space.xxs
            anchors.bottomMargin: Theme.space.xxs
            radius: Theme.radius.sm
            color: row.selected ? Theme.palette.accentSubtle
                 : row.hovered ? Theme.palette.surfaceRaised : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
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
        // The download's progress along the row's foot.
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

        Rectangle {
            Layout.preferredWidth: row.logoSize
            Layout.preferredHeight: row.logoSize
            radius: Theme.radius.sm
            color: Theme.palette.surfaceSunken
            clip: true
            Image {
                anchors.fill: parent
                anchors.margins: 1
                source: row.logoUrl
                sourceSize: Qt.size(row.logoSourceSize, row.logoSourceSize)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xxs

            // "Sodium by jellysquid3", the way Modrinth names a result: the
            // title keeps its room, the author gives way first.
            // Placed by hand: a layout sizing the title from its own width
            // looped.
            Item {
                Layout.fillWidth: true
                implicitHeight: titleText.implicitHeight
                Text {
                    id: titleText
                    width: Math.min(implicitWidth, parent.width)
                    text: row.title
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    font.weight: Font.DemiBold
                }
                Text {
                    x: titleText.width + Theme.space.xs
                    width: Math.max(0, parent.width - x)
                    anchors.baseline: titleText.baseline
                    visible: row.author.length > 0 && width > 0
                    text: qsTr("by %1").arg(row.author)
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
            Text {
                Layout.fillWidth: true
                text: row.downloading ? (row.download.status || qsTr("Installing…"))
                    : row.checking ? qsTr("Checking what it needs…") : row.description
                elide: Text.ElideRight
                color: row.downloading || row.checking ? Theme.palette.accentText : Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        StatusBadge {
            visible: row.installed || row.justInstalled || row.failed
            tone: row.failed ? "danger" : "success"
            text: row.failed ? qsTr("Failed") : qsTr("Installed")
        }
    }
}
