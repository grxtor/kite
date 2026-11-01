// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Change version…" for one installed file: every version of its project for this instance, listed as in Add
 * content (VersionRow), the installed one tagged. Install hands a ContentInstallReview back through
 * reviewRequested(), so what it needs comes along and the old file is replaced.
 */
AppDialog {
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
    readonly property int preferredHeight: 560
    readonly property int listMinHeight: 200

    height: Math.min(root.preferredHeight, parent ? parent.height - Theme.space.xxl * 2 : root.preferredHeight)
    closeOnOutsidePress: true
    title: root.controller ? qsTr("Change version of %1").arg(root.controller.versionsTitle) : ""

    // Preselects the newest compatible version that is not the installed one.
    onVersionsChanged: {
        let pick = root.versions.findIndex((v) => v.isCompatible && v.id !== root.installedId)
        if (pick < 0)
            pick = root.versions.findIndex((v) => v.id === root.installedId)
        root.pickedVersion = pick
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        PanelNote {
            Layout.fillWidth: true
            text: qsTr("Versions that run on this instance come first. Anything the new version needs is listed before it installs.")
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: root.listMinHeight

            ListView {
                id: versionList
                anchors.fill: parent
                clip: true
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
                    lastRow: index === versionList.count - 1
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
                MeshIcon { iconName: "layers"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
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
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }
}
