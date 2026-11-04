// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Kite.Theme

/*
 * Where this instance's modpack came from and what other versions exist: the
 * installed version, a drop-down of the others with an Update button, and the
 * changelog of the picked one.
 */
Item {
    id: root

    // InstanceDetails.managedPack (ManagedPackController), or null.
    property var controller: null
    property var watcher: null
    readonly property bool updating: !!root.watcher && root.watcher.running

    readonly property var versions: root.controller ? root.controller.versions : []
    readonly property bool hasVersions: !!root.controller && root.controller.hasPackId && root.versions.length > 0
    readonly property var picked: root.hasVersions && versionBox.currentIndex >= 0
                                  && versionBox.currentIndex < root.versions.length
                                  ? root.versions[versionBox.currentIndex] : null
    readonly property var versionLabels: root.versions.map((v) => {
        var label = String(v.name || v.label)
        if (v.minecraft)
            label = qsTr("%1, Minecraft %2").arg(label).arg(v.minecraft)
        if (v.releaseType === "alpha")
            label = qsTr("%1, alpha").arg(label)
        else if (v.releaseType === "beta")
            label = qsTr("%1, beta").arg(label)
        return v.current ? qsTr("%1 (installed)").arg(label) : label
    })
    readonly property int versionBoxWidth: 360

    // Emitted once an update has landed on disk: this instance was just replaced, so the page
    // showing it (including this controller) is stale.
    signal updated()

    // Fires at creation (the initial binding counts as a change from `null`) and whenever the page
    // opens another instance; fetchVersions() is a method, so the fetch is explicit.
    onControllerChanged: {
        root.watcher = null
        if (root.controller)
            root.controller.fetchVersions()
    }

    Connections {
        target: root.watcher
        ignoreUnknownSignals: true
        function onFinished(ok) {
            if (ok)
                root.updated()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: !!root.controller

        TabToolbar {
            Layout.fillWidth: true

            leading: [
                Tag { text: root.controller ? root.controller.providerLabel : "" },
                Copy {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: root.controller && root.controller.installedVersionName.length > 0
                          ? qsTr("Installed %1").arg(root.controller.installedVersionName)
                          : qsTr("Installed version unknown")
                    tone: "secondary"
                }
            ]

            Button {
                text: qsTr("Website")
                icon.source: Icons.url("external-link")
                visible: root.controller && root.controller.packUrl.length > 0
                onClicked: ExternalLinks.open(root, root.controller.packUrl)
            }
            HoverReason {
                reason: root.controller && root.controller.running
                        ? qsTr("The game is running. The pack can be updated once it closes.") : ""
                Button {
                    enabled: !root.updating && !!root.controller && !root.controller.running
                    text: qsTr("Update from file…")
                    icon.source: Icons.url("archive")
                    onClicked: fileDialog.open()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.updating || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.md

            Copy {
                Layout.fillWidth: true
                text: root.watcher
                      ? (root.watcher.failed ? (root.watcher.error || qsTr("Update failed."))
                                             : (root.watcher.status || qsTr("Updating…")))
                      : ""
                role: "label"
                tone: root.watcher && root.watcher.failed ? "danger" : "secondary"
                elide: Text.ElideRight
            }
            LaunchProgressBar {
                Layout.preferredWidth: 160
                visible: root.updating
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !!root.controller && !root.controller.hasPackId
            tone: "info"
            text: qsTr("Kite does not have a catalogue id on record for this pack, so it cannot list other versions here. You can still update it from a pack file, or install the pack again from Discover for a specific version.")
        }

        // Plain words only: the catalogue's own message is a raw job string meant for the log.
        NoticeBar {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.hasPackId && root.controller.error.length > 0
            text: qsTr("Could not load the versions of this pack. Check your connection, then reload.")
            actionText: qsTr("Reload")
            onActionTriggered: root.controller.reload()
        }

        Copy {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.hasPackId && root.controller.error.length === 0
                     && !root.controller.loading && root.versions.length === 0
            wrapMode: Text.Wrap
            text: qsTr("The catalogue lists no other versions of this pack.")
            tone: "secondary"
        }

        // The newest is picked first, so its changelog is there to read before updating.
        BoxSurface {
            Layout.fillWidth: true
            visible: root.hasVersions
            implicitHeight: updateRow.implicitHeight + Theme.space.md * 2

            RowLayout {
                id: updateRow
                anchors.fill: parent
                anchors.leftMargin: Theme.space.lg
                anchors.rightMargin: Theme.space.md
                spacing: Theme.space.sm

                Copy {
                    text: qsTr("Update to")
                    role: "label"
                    tone: "secondary"
                }
                ComboBox {
                    id: versionBox
                    Layout.preferredWidth: root.versionBoxWidth
                    model: root.versionLabels
                    Accessible.name: qsTr("Version")
                }
                Item { Layout.fillWidth: true }
                Button {
                    enabled: !root.updating && !!root.picked && root.picked.installable && !root.picked.current
                    highlighted: enabled
                    text: qsTr("Update")
                    icon.source: Icons.url("download")
                    onClicked: root.watcher = root.controller.updateToVersion(versionBox.currentIndex)
                }
            }
        }

        BoxSurface {
            id: changelogHost
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.hasVersions && !!root.picked
            sunken: true

            Flickable {
                anchors.fill: parent
                anchors.margins: Theme.space.md
                contentWidth: width
                contentHeight: changelog.implicitHeight
                clip: true
                ScrollBar.vertical: ScrollBar {}

                // RichBody's article (ManagedPackController): its links are http(s) or mail only.
                RemoteRichText {
                    id: changelog
                    width: parent.width
                    format: "html"
                    article: true
                    source: !root.picked ? ""
                            : root.picked.changelog.length > 0
                            ? root.picked.changelog
                            : qsTr("No changelog available for this version.")
                    onLinkActivated: (link) => ExternalLinks.open(changelog, link)
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !changelogHost.visible
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose the pack file")
        nameFilters: root.controller ? [root.controller.fileFilter] : []
        onAccepted: root.watcher = root.controller.updateFromFile(selectedFile)
    }

    BusyIndicator {
        anchors.centerIn: parent
        visible: root.controller && root.controller.loading
        running: visible
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: !root.controller
        title: qsTr("Not a managed pack")
        body: qsTr("This instance was not installed from a catalogue Kite recognises.")
        MeshIcon { iconName: "package"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }
}
