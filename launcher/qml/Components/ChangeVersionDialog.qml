// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * "Change version…" for one installed file: every version of its project
 * that the provider has for this instance, listed the way Add content
 * lists them (VersionRow), the installed one tagged. Picking another and
 * pressing Install hands a ContentInstallReview back through
 * reviewRequested() -- the same path an Add content install takes, so
 * what it needs comes along and the old file is replaced in place.
 * The QML replacement for ModFolderPage's version-change dialog.
 */
Dialog {
    id: root

    // ContentUpdateController, already asked to loadVersions().
    property var controller: null
    signal reviewRequested(var review)

    property int pickedVersion: -1
    readonly property var versions: root.controller ? root.controller.versions : []
    readonly property bool loading: !!root.controller && root.controller.versionsLoading
    readonly property string installedId: root.controller ? root.controller.installedVersionId : ""
    readonly property bool pickedInstalled: root.pickedVersion >= 0 && root.pickedVersion < root.versions.length
                                            && root.versions[root.pickedVersion].id === root.installedId
    readonly property int preferredWidth: 520
    readonly property int preferredHeight: 560
    readonly property int listMinHeight: 200

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.preferredHeight, parent ? parent.height - Theme.space.xxl * 2 : root.preferredHeight)
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: root.controller ? qsTr("Change version of %1").arg(root.controller.versionsTitle) : ""

    header: DialogHeader {
        title: root.title
        icon: "layers"
    }

    // The newest compatible one that is not the installed one, so the
    // likely choice is already picked.
    onVersionsChanged: {
        let pick = root.versions.findIndex((v) => v.isCompatible && v.id !== root.installedId)
        if (pick < 0)
            pick = root.versions.findIndex((v) => v.id === root.installedId)
        root.pickedVersion = pick
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: qsTr("Versions that run on this instance come first. Anything the new version needs is listed before it installs.")
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.listMinHeight
            radius: Theme.radius.md
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            ListView {
                id: versionList
                anchors.fill: parent
                anchors.margins: Theme.space.xs
                clip: true
                spacing: Theme.space.xxs
                model: root.versions
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: VersionRow {
                    required property int index
                    required property var modelData
                    width: versionList.width - Theme.space.sm
                    entry: modelData
                    picked: root.pickedVersion === index
                    installed: modelData.id === root.installedId
                    onClicked: root.pickedVersion = index
                }
            }

            BusyIndicator {
                anchors.centerIn: parent
                running: root.loading
                visible: running
            }

            EmptyState {
                anchors.centerIn: parent
                width: parent.width
                visible: !root.loading && root.versions.length === 0
                title: qsTr("No versions")
                body: root.controller ? root.controller.versionsError : ""
                MeshIcon { iconName: "layers"; size: 40; color: Theme.palette.textTertiary }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            highlighted: true
            enabled: !root.loading && root.pickedVersion >= 0 && !root.pickedInstalled
            text: root.pickedInstalled ? qsTr("Already installed") : qsTr("Install this version")
            icon.source: Icons.url("download")
            onClicked: {
                const review = root.controller.reviewVersion(root.versions[root.pickedVersion].id)
                root.close()
                if (review)
                    root.reviewRequested(review)
            }
        }
        Button {
            flat: true
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
