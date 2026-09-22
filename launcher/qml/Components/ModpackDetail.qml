// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One modpack, opened from Discover: what it is, which version to install,
 * and the install itself -- progress in place, then a way to the new
 * instance. `pack` is the row that was opened (title, author, logo...);
 * `detail` is the model's lazily loaded body and version list.
 */
Item {
    id: root

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher. The shell's, so
    // the install is owned by C++ and survives leaving this page.
    property var installer: null
    // TaskWatcher of the install started from here, if any.
    property var watcher: null

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded

    function versionLabel(v) {
        var parts = []
        if (v.gameVersions && v.gameVersions.length > 0)
            parts.push(v.gameVersions[0])
        if (v.loaders && v.loaders.length > 0)
            parts.push(v.loaders.map(l => l.charAt(0).toUpperCase() + l.slice(1)).join(", "))
        return (v.versionNumber || v.name) + (parts.length > 0 ? "  ·  " + parts.join(" · ") : "")
    }

    function install() {
        if (!root.installer || versionBox.currentIndex < 0)
            return
        var v = root.versions[versionBox.currentIndex]
        root.watcher = root.installer(root.pack.projectId, v.id,
                                      nameField.text.length > 0 ? nameField.text : root.pack.title, "")
    }

    onPackChanged: {
        watcher = null
        nameField.text = pack.title || ""
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: Theme.space.xl + Theme.space.xs
            width: Math.min(parent.width - x * 2, 960)
            spacing: Theme.space.xl

            Button {
                flat: true
                text: qsTr("Back to results")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                onClicked: root.backRequested()
            }

            // Header
            RowLayout {
                width: parent.width
                spacing: Theme.space.xl

                Rectangle {
                    Layout.preferredWidth: 112
                    Layout.preferredHeight: 112
                    radius: Theme.radius.xl
                    color: Theme.palette.surfaceSunken
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: root.pack.logoUrl || ""
                        sourceSize: Qt.size(224, 224)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm

                    Text {
                        width: parent.width
                        text: root.pack.title || ""
                        wrapMode: Text.Wrap
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.display.pixelSize
                        font.weight: Font.Bold
                        font.letterSpacing: -0.4
                    }
                    Text {
                        visible: !!root.pack.author
                        text: qsTr("by %1").arg(root.pack.author || "")
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                    }
                    Row {
                        spacing: Theme.space.sm
                        Tag {
                            iconName: "download"
                            text: qsTr("%1 downloads").arg(Format.compactNumber(root.pack.downloads))
                        }
                        Tag {
                            iconName: "globe"
                            text: "Modrinth"
                        }
                    }
                }
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.pack.description || ""
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize + 1
                lineHeight: 1.35
            }

            // Install
            SettingsGroup {
                width: parent.width
                title: qsTr("Install")

                SettingRow {
                    label: qsTr("Version")
                    description: root.detail && root.detail.loading ? qsTr("Loading versions…")
                               : root.versions.length === 0 ? qsTr("No versions found.") : ""
                    ComboBox {
                        id: versionBox
                        width: 320
                        enabled: root.versions.length > 0 && !root.installing
                        model: root.versions.map(v => root.versionLabel(v))
                        // Newest first from Modrinth, alphas included: start on
                        // the version the author features, if there is one.
                        onModelChanged: {
                            var featured = root.versions.findIndex(v => v.featured)
                            currentIndex = featured >= 0 ? featured : 0
                        }
                    }
                }

                SettingRow {
                    label: qsTr("Instance name")
                    TextField {
                        id: nameField
                        width: 320
                        enabled: !root.installing
                        selectByMouse: true
                    }
                }

                SettingRow {
                    label: root.installed ? qsTr("Installed")
                         : root.installing ? (root.watcher.status || qsTr("Installing…"))
                         : root.watcher && root.watcher.failed ? qsTr("Install failed")
                         : qsTr("Ready to install")
                    description: root.watcher && root.watcher.failed ? root.watcher.error
                               : root.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""

                    Row {
                        spacing: Theme.space.md

                        LaunchProgressBar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 180
                            visible: root.installing
                            progress: root.watcher ? root.watcher.progress : -1
                        }

                        Button {
                            visible: !root.installed
                            enabled: !root.installing && versionBox.currentIndex >= 0
                            highlighted: true
                            text: root.installing ? qsTr("Installing…") : qsTr("Install")
                            icon.source: Icons.url("download")
                            onClicked: root.install()
                        }

                        Button {
                            visible: root.installed
                            highlighted: true
                            text: qsTr("Show in library")
                            icon.source: Icons.url("library")
                            onClicked: root.showInstanceRequested(root.watcher.instanceId || "")
                        }
                    }
                }
            }

            // About
            Column {
                width: parent.width
                spacing: Theme.space.md
                visible: !!root.detail && ((root.detail.body || "").length > 0 || root.detail.loading)

                Text {
                    leftPadding: Theme.space.xs
                    text: qsTr("About")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                BusyIndicator {
                    visible: !!root.detail && root.detail.loading
                    running: visible
                }

                Text {
                    width: parent.width
                    text: root.detail ? (root.detail.body || "") : ""
                    textFormat: Text.MarkdownText
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                    linkColor: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }
    }
}
