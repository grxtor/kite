// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The side column of a modpack's page (ModpackDetail), the same on every tab: the Install card (version, instance
 * name and the page's one Install button, with its progress and what became of it), what the pack runs on, its
 * links, and the plain facts. `page` is the ModpackDetail, which owns the install.
 */
ColumnLayout {
    id: root

    required property var page
    readonly property var pack: page.pack
    readonly property var detail: page.detail
    readonly property var versions: page.versions
    readonly property var watcher: page.watcher

    property alias versionIndex: versionBox.currentIndex
    property alias instanceName: nameField.text
    readonly property int maxCompatVersions: 6

    readonly property var linkLabels: ({ website: qsTr("Project page"), source: qsTr("Source code"),
                                         issues: qsTr("Issue tracker"), wiki: qsTr("Wiki"), discord: qsTr("Discord") })
    // The loaders' own marks, shipped with the launcher's instance icons.
    readonly property var loaderMarks: ({ fabric: "fabricmc", forge: "forge_mark", neoforge: "neoforged",
                                          quilt: "quilt_mark" })

    spacing: Theme.space.md

    function openVersions() {
        versionBox.popup.open()
    }

    component SideCard: BoxSurface {
        id: sideCard
        property string title
        default property alias content: sideBody.data
        implicitHeight: sideColumn.implicitHeight + Theme.space.lg * 2

        Column {
            id: sideColumn
            x: Theme.space.lg
            y: Theme.space.lg
            width: parent.width - Theme.space.lg * 2
            spacing: Theme.space.md

            Copy {
                text: sideCard.title
                role: "subheading"
            }
            Column {
                id: sideBody
                width: parent.width
                spacing: Theme.space.md
            }
        }
    }

    component Fact: Column {
        property string label
        property string value
        visible: value.length > 0
        width: parent ? parent.width : 0
        spacing: Theme.space.xxs
        Copy {
            text: parent.label
            role: "caption"
            tone: "tertiary"
            font.weight: Font.Medium
        }
        Copy {
            width: parent.width
            text: parent.value
            wrapMode: Text.Wrap
            role: "label"
        }
    }

    SideCard {
        Layout.fillWidth: true
        title: qsTr("Install")

        Column {
            width: parent.width
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Version") }
            ComboBox {
                id: versionBox
                width: parent.width
                enabled: root.versions.length > 0 && !root.page.installing
                model: root.versions.map(v => root.page.versionLabel(v))
                displayText: root.detail && root.detail.loading && root.versions.length === 0
                             ? qsTr("Loading versions…")
                             : root.versions.length === 0 ? qsTr("No versions found") : currentText
                Accessible.name: qsTr("Version")
                // Newest first from either provider, alphas included: start on the version the author features.
                onModelChanged: {
                    var featured = root.versions.findIndex(v => v.featured)
                    currentIndex = featured >= 0 ? featured : 0
                }
            }
        }

        Column {
            width: parent.width
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Instance name") }
            TextField {
                id: nameField
                width: parent.width
                enabled: !root.page.installing
                selectByMouse: true
                Accessible.name: qsTr("Instance name")
                onAccepted: if (installButton.enabled) root.page.install()
            }
        }

        Column {
            width: parent.width
            visible: !!root.watcher
            spacing: Theme.space.xs

            Copy {
                width: parent.width
                text: root.page.installed ? qsTr("Installed")
                    : root.page.installing ? (root.watcher.status || qsTr("Installing…"))
                    : root.watcher && root.watcher.failed ? qsTr("Install failed")
                    : ""
                wrapMode: Text.Wrap
                role: "label"
                font.weight: Font.Medium
            }
            LaunchProgressBar {
                width: parent.width
                visible: root.page.installing
                progress: root.watcher ? root.watcher.progress : -1
            }
            Copy {
                width: parent.width
                visible: text.length > 0
                text: root.watcher && root.watcher.failed ? root.watcher.error
                    : root.page.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""
                wrapMode: Text.Wrap
                role: "caption"
                tone: "secondary"
            }
        }

        // The page's one primary action; once the pack is installed it leads to the new instance.
        Button {
            id: installButton
            width: parent.width
            highlighted: true
            enabled: root.page.installed || (!root.page.installing && versionBox.currentIndex >= 0 && root.versions.length > 0)
            text: root.page.installed ? qsTr("Show in library")
                : root.page.installing ? qsTr("Installing…")
                : root.watcher && root.watcher.failed ? qsTr("Try again") : qsTr("Install")
            icon.source: Icons.url(root.page.installed ? "library" : "download")
            onClicked: root.page.installed ? root.page.showInstanceRequested(root.watcher.instanceId || "")
                                           : root.page.install()
        }
    }

    SideCard {
        Layout.fillWidth: true
        visible: root.page.gameVersions.length > 0 || root.page.loaderIds.length > 0
        title: qsTr("Compatibility")

        Column {
            width: parent.width
            visible: root.page.gameVersions.length > 0
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Minecraft") }
            Flow {
                width: parent.width
                spacing: Theme.space.xs
                Repeater {
                    model: root.page.gameVersions.slice(0, root.maxCompatVersions)
                    delegate: Tag {
                        required property var modelData
                        text: String(modelData)
                    }
                }
                Tag {
                    visible: root.page.gameVersions.length > root.maxCompatVersions
                    text: qsTr("+%1").arg(root.page.gameVersions.length - root.maxCompatVersions)
                }
            }
        }
        Column {
            width: parent.width
            visible: root.page.loaderIds.length > 0
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Loaders") }
            Flow {
                width: parent.width
                spacing: Theme.space.md
                Repeater {
                    model: root.page.loaderIds
                    delegate: Row {
                        required property var modelData
                        readonly property string mark: root.loaderMarks[String(modelData)] || ""
                        spacing: Theme.space.xs
                        Image {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: parent.mark.length > 0
                            width: Theme.icon.sm
                            height: Theme.icon.sm
                            source: parent.mark.length > 0 ? Icons.instanceIcon(parent.mark) : ""
                            sourceSize: Qt.size(width * 2, height * 2)
                            fillMode: Image.PreserveAspectFit
                        }
                        Copy {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.page.loaderName(String(modelData))
                            role: "label"
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }
    }

    SideCard {
        Layout.fillWidth: true
        readonly property var links: root.detail && root.detail.links ? root.detail.links : []
        visible: links.length > 0
        title: qsTr("Links")

        Column {
            width: parent.width
            spacing: 0
            Repeater {
                model: root.detail && root.detail.links ? root.detail.links : []
                delegate: Button {
                    required property var modelData
                    // Flush with the card's text: the ghost button's padding hangs outside it.
                    x: -leftPadding
                    flat: true
                    text: root.linkLabels[modelData.kind] || modelData.kind
                    icon.source: Icons.url("external-link")
                    onClicked: ExternalLinks.open(root, modelData.url)
                }
            }
        }
    }

    SideCard {
        Layout.fillWidth: true
        title: qsTr("Details")

        Fact {
            label: qsTr("Author")
            value: root.pack.author || ""
        }
        Fact {
            label: qsTr("Downloads")
            value: (root.pack.downloads || 0) > 0 ? Number(root.pack.downloads).toLocaleString(Qt.locale(), "f", 0) : ""
        }
        Fact {
            label: qsTr("Updated")
            value: {
                var ms = Date.parse(root.pack.updated || "")
                return ms > 0 ? Format.calendarDate(new Date(ms)) : ""
            }
        }
        Fact {
            label: qsTr("License")
            value: root.detail && root.detail.license ? root.detail.license : ""
        }
    }
}
