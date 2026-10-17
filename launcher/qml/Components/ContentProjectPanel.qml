// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The project picked in ContentBrowserView: a compact header, three lines
 * of description, then its versions -- which get every pixel the rest does
 * not need -- the newest compatible one preselected, and Install. Shown
 * beside the results only once something is picked; × closes it.
 */
Rectangle {
    id: root

    // The ContentBrowserView: selectedProject, versions, pickedVersion, the
    // selected project's download and install().
    required property var owner
    readonly property var project: owner.selectedProject
    readonly property var download: owner.selectedDownload
    signal closeRequested()

    readonly property int logoSize: 48
    // Pixels of the logo image asked for: twice its size, for HiDPI.
    readonly property int logoSourceSize: logoSize * 2
    // The least the version list shrinks to: two rows.
    readonly property int versionListMinHeight: 96
    readonly property int descriptionLines: 3
    readonly property int descriptionLinesExpanded: 12
    // Below this height (a small window) the description gives its room to
    // the versions; Install always stays in view.
    readonly property int roomForDescription: 420

    radius: Theme.radius.lg
    color: Theme.palette.surface
    border.width: 1
    border.color: Theme.palette.border

    // A new pick starts with its description folded again.
    onProjectChanged: descriptionText.expanded = false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md
            Rectangle {
                Layout.preferredWidth: root.logoSize
                Layout.preferredHeight: root.logoSize
                Layout.alignment: Qt.AlignTop
                radius: Theme.radius.sm
                color: Theme.palette.surfaceSunken
                clip: true
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.project ? root.project.logoUrl : ""
                    sourceSize: Qt.size(root.logoSourceSize, root.logoSourceSize)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs
                Text {
                    Layout.fillWidth: true
                    text: root.project ? root.project.title : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.project && root.project.author ? qsTr("by %1").arg(root.project.author) : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                iconName: "x"
                tip: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }

        // Three lines, the rest on request: the version list is what this
        // panel is for.
        Text {
            id: descriptionText
            property bool expanded: false
            Layout.fillWidth: true
            visible: text.length > 0 && root.height >= root.roomForDescription
            text: root.project ? root.project.description : ""
            wrapMode: Text.Wrap
            maximumLineCount: expanded ? root.descriptionLinesExpanded : root.descriptionLines
            elide: Text.ElideRight
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
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
            Text {
                Layout.fillWidth: true
                text: qsTr("Versions")
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.bodyStrong.pixelSize
                font.weight: Theme.type.bodyStrong.weight
            }
            Text {
                visible: root.owner.versions.length > 0
                text: qsTr("%n available", "", root.owner.versions.length)
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.versionListMinHeight
            radius: Theme.radius.sm
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: versionList
                anchors.fill: parent
                anchors.margins: Theme.space.xs
                clip: true
                spacing: Theme.space.xxs
                model: root.owner.versions
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: VersionRow {
                    required property int index
                    required property var modelData
                    width: versionList.width - Theme.space.sm
                    entry: modelData
                    picked: root.owner.pickedVersion === index
                    onClicked: root.owner.pickedVersion = index
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: !!root.owner.browser && root.owner.browser.versionsLoading
                    visible: running
                }
            }
        }

        Text {
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
            color: root.owner.installNote.length > 0 || (d && d.failed) ? Theme.palette.danger
                 : d && d.succeeded && !root.owner.selectedChecking ? Theme.palette.success
                 : Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
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
