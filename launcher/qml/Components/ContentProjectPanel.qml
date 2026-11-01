// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The project picked in ContentBrowserView: a compact header, three lines of description, then its versions (the
 * newest compatible one preselected) and Install. Shown beside the results only once something is picked.
 */
BoxSurface {
    id: root

    // The ContentBrowserView.
    required property var owner
    readonly property var project: owner.selectedProject
    readonly property var download: owner.selectedDownload
    signal closeRequested()

    readonly property int logoSize: 48
    readonly property int logoSourceSize: logoSize * 2
    readonly property int versionListMinHeight: 96
    readonly property int descriptionLines: 3
    readonly property int descriptionLinesExpanded: 12
    // Below this height the description gives its room to the versions; Install always stays in view.
    readonly property int roomForDescription: 420

    onProjectChanged: descriptionText.expanded = false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md
            Item {
                Layout.preferredWidth: root.logoSize
                Layout.preferredHeight: root.logoSize
                Layout.alignment: Qt.AlignTop

                Image {
                    anchors.fill: parent
                    source: root.project ? root.project.logoUrl : ""
                    sourceSize: Qt.size(root.logoSourceSize, root.logoSourceSize)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
                CornerMatte {
                    cornerRadius: Theme.radius.md
                    matte: root.color
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs
                Copy {
                    Layout.fillWidth: true
                    text: root.project ? root.project.title : ""
                    elide: Text.ElideRight
                    role: "subheading"
                }
                Copy {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.project && root.project.author ? qsTr("by %1").arg(root.project.author) : ""
                    elide: Text.ElideRight
                    role: "label"
                    tone: "tertiary"
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                iconName: "x"
                tip: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }

        Copy {
            id: descriptionText
            property bool expanded: false
            Layout.fillWidth: true
            visible: text.length > 0 && root.height >= root.roomForDescription
            text: root.project ? root.project.description : ""
            wrapMode: Text.Wrap
            maximumLineCount: expanded ? root.descriptionLinesExpanded : root.descriptionLines
            elide: Text.ElideRight
            role: "label"
            tone: "secondary"
            lineHeight: Theme.type.label.lineHeight
            MouseArea {
                anchors.fill: parent
                enabled: descriptionText.truncated || descriptionText.expanded
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: descriptionText.expanded = !descriptionText.expanded
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Copy {
                Layout.fillWidth: true
                text: qsTr("Versions")
                role: "bodyStrong"
            }
            Copy {
                visible: root.owner.versions.length > 0
                text: qsTr("%n available", "", root.owner.versions.length)
                role: "caption"
                tone: "tertiary"
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.versionListMinHeight

            ListView {
                id: versionList
                anchors.fill: parent
                clip: true
                model: root.owner.versions
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: VersionRow {
                    required property int index
                    required property var modelData
                    width: versionList.width - Theme.space.sm
                    entry: modelData
                    picked: root.owner.pickedVersion === index
                    lastRow: index === versionList.count - 1
                    onClicked: root.owner.pickedVersion = index
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: !!root.owner.browser && root.owner.browser.versionsLoading
                    visible: running
                }
            }
        }

        Copy {
            Layout.fillWidth: true
            visible: text.length > 0
            readonly property var d: root.download
            text: root.owner.installNote.length > 0 ? root.owner.installNote
                : root.owner.selectedChecking ? qsTr("Checking what it needs…")
                : d && d.failed ? (d.error || qsTr("Install failed."))
                : d && d.succeeded ? qsTr("Installed.")
                : d ? (d.status || "") : ""
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            role: "label"
            tone: root.owner.installNote.length > 0 || (d && d.failed) ? "danger"
                : d && d.succeeded && !root.owner.selectedChecking ? "success"
                : "secondary"
        }
        LaunchProgressBar {
            Layout.fillWidth: true
            visible: root.owner.selectedInstalling
            progress: root.download ? root.download.progress : -1
        }

        HoverReason {
            Layout.fillWidth: true
            reason: root.owner.blockedReason
            Button {
                width: parent ? parent.width : implicitWidth
                highlighted: true
                enabled: !root.owner.selectedInstalling && !root.owner.selectedChecking
                         && root.owner.pickedVersion >= 0 && !!root.owner.browser
                         && root.owner.blockedReason.length === 0
                text: root.owner.selectedInstalling ? qsTr("Installing…")
                    : root.owner.selectedChecking ? qsTr("Checking…")
                    : root.project && root.project.installed ? qsTr("Install this version")
                    : qsTr("Install")
                icon.source: Icons.url("download")
                onClicked: root.owner.install()
            }
        }
    }
}
