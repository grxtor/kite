// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One search result of ContentBrowserView: logo, "Title by author", one line of description (or the install's
 * progress), a quiet line of facts (downloads, last update, up to three categories), then "Install" (the version
 * the project's panel preselects), or "Installed" once it is. A tick box slides in before the logo while the
 * pointer is over the list or anything is ticked (`ticking`), for installing several at once. Rows share one card,
 * split by hairlines; hover and the picked row share one look, a lifted inset strip with no hairline and no accent.
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
    required property string projectId
    required property var downloads
    required property string updated
    required property var categories
    required property string websiteUrl
    required property var gallery

    // The ContentBatchInstall, and whether the tick boxes show.
    property var batch: null
    property bool ticking: false
    readonly property bool ticked: !!batch && batch.selected.length >= 0 && batch.isSelected(projectId)
    readonly property bool tickable: !installed && !justInstalled && !downloading && !checking
    readonly property int checkSize: 20
    readonly property string updatedText: Format.ageText(updated)

    readonly property bool selected: owner.selectedRow === index
    readonly property string key: owner.projectKey(title, author)
    readonly property var download: owner.downloadFor(key)
    readonly property bool downloading: !!download && download.running
    readonly property bool checking: owner.isChecking(key)
    readonly property bool justInstalled: !!download && download.succeeded
    readonly property bool failed: !!download && download.failed
    property bool lastRow: false
    // The row below is hovered or picked: its lifted strip meets this row's edge, so the hairline goes.
    property bool nextLifted: false

    readonly property int logoSize: 40
    readonly property int logoSourceSize: logoSize * 2
    readonly property int rowHeight: 92

    implicitHeight: rowHeight
    leftPadding: Theme.space.lg
    rightPadding: Theme.space.md
    hoverEnabled: true
    readonly property var project: ({ title: title, author: author, description: description,
                                      logoUrl: logoUrl, installed: installed, websiteUrl: websiteUrl,
                                      gallery: gallery, downloads: downloads, updated: updated })
    onClicked: owner.select(index, project)

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
            visible: !row.lastRow && !row.hovered && !row.selected && !row.nextLifted
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

        // Grows from nothing, taking its share of the spacing with it, so the logo slides aside once for the
        // whole list rather than under each row's pointer.
        Item {
            readonly property real reveal: row.ticking ? 1 : 0
            Layout.preferredWidth: row.checkSize * shown
            Layout.preferredHeight: row.checkSize
            Layout.rightMargin: -Theme.space.md * (1 - shown)
            property real shown: reveal
            Behavior on shown { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            opacity: shown
            visible: shown > 0
            clip: true
            CheckBox {
                anchors.verticalCenter: parent.verticalCenter
                width: row.checkSize
                padding: 0
                visible: row.tickable || row.ticked
                checked: row.ticked
                Accessible.name: qsTr("Select %1").arg(row.title)
                onToggled: {
                    row.batch.toggle(row.projectId, row.title, row.author)
                    checked = Qt.binding(() => row.ticked)
                }
            }
        }

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
            ContentResultMeta {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xxs
                downloads: row.downloads
                updatedText: row.updatedText
                categories: row.categories
            }
        }

        // As tall as the Install button it replaces, so the trailing column keeps one line.
        Item {
            visible: row.installed || row.justInstalled || row.failed
            Layout.preferredWidth: stateBadge.implicitWidth
            Layout.preferredHeight: Theme.control.heightSm
            StatusBadge {
                id: stateBadge
                anchors.centerIn: parent
                tone: row.failed ? "danger" : "success"
                text: row.failed ? qsTr("Failed") : qsTr("Installed")
            }
        }
        HoverReason {
            visible: !row.installed && !row.justInstalled
            reason: owner.blockedReason
            Button {
                enabled: !row.downloading && !row.checking && owner.blockedReason.length === 0
                // Outlined at rest; the row under the pointer shows its one filled action.
                highlighted: row.hovered && enabled
                buttonSize: "sm"
                text: row.downloading || row.checking ? qsTr("Installing…")
                    : row.failed ? qsTr("Try again") : qsTr("Install")
                icon.source: Icons.url("download")
                Accessible.name: qsTr("Install %1").arg(row.title)
                onClicked: owner.quickInstall(row.index, row.project)
            }
        }
    }
}
