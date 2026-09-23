// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Find mods, resource packs, shaders and data packs for this instance on
 * Modrinth or CurseForge, and install them -- dependencies included --
 * without leaving the instance. Results already match the instance's
 * Minecraft version and loader; things already installed say so.
 *
 * Results on the left, the picked project on the right with its versions,
 * the newest compatible one preselected.
 */
Item {
    id: root

    // ContentBrowser of the open instance.
    property var browser: null
    // (row, versionId) -> TaskWatcher, C++-owned.
    property var installer: null
    property int selectedRow: -1
    property var selectedProject: null
    property var watcher: null
    property int pickedVersion: -1

    readonly property var versions: browser && browser.versions ? browser.versions : []
    readonly property bool installing: !!watcher && watcher.running

    readonly property var providers: [
        { value: "modrinth", label: "Modrinth" },
        { value: "curseforge", label: "CurseForge" }
    ]
    readonly property var types: [
        { value: "mods", label: qsTr("Mods") },
        { value: "resourcepacks", label: qsTr("Resource packs") },
        { value: "shaderpacks", label: qsTr("Shaders") },
        { value: "datapacks", label: qsTr("Data packs") }
    ]

    property bool searched: false
    function runSearch() {
        if (!root.browser)
            return
        root.searched = true
        root.selectedRow = -1
        root.selectedProject = null
        root.browser.search()
    }
    function searchIfFirstShown() {
        if (visible && !root.searched)
            runSearch()
    }
    onVisibleChanged: searchIfFirstShown()
    onBrowserChanged: { root.searched = false; searchIfFirstShown() }

    function select(row, project) {
        root.selectedRow = row
        root.selectedProject = project
        root.watcher = null
        root.pickedVersion = -1
        root.browser.loadVersions(row)
    }

    onVersionsChanged: {
        var compatible = root.versions.findIndex(v => v.isCompatible)
        root.pickedVersion = compatible >= 0 ? compatible : (root.versions.length > 0 ? 0 : -1)
    }

    Timer {
        id: debounce
        interval: 350
        onTriggered: {
            root.browser.query = searchField.text
            root.runSearch()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            SegmentedControl {
                options: root.providers
                current: root.browser ? root.browser.provider : "modrinth"
                onActivated: (value) => { root.browser.provider = value; root.runSearch() }
            }
            SegmentedControl {
                options: root.types
                current: root.browser ? root.browser.contentType : "mods"
                onActivated: (value) => { root.browser.contentType = value; root.runSearch() }
            }
            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            SearchBox {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search")
                onTextEdited: debounce.restart()
                onTextChanged: if (text.length === 0) debounce.restart()
            }
            ComboBox {
                Layout.preferredWidth: 200
                visible: count > 0
                model: root.browser && root.browser.sortOptions ? root.browser.sortOptions : []
                textRole: "label"
                valueRole: "id"
                onActivated: { root.browser.sortIndex = currentValue; root.runSearch() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.lg

            // Results
            ListView {
                id: results
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                clip: true
                spacing: Theme.space.xs
                boundsBehavior: Flickable.StopAtBounds
                model: root.browser ? root.browser.results : null
                ScrollBar.vertical: ScrollBar {}
                onAtYEndChanged: if (atYEnd && root.browser && root.browser.canFetchMore && !root.browser.searching)
                                     root.browser.fetchMore()

                delegate: AbstractButton {
                    id: row
                    required property int index
                    required property string title
                    required property string description
                    required property string author
                    required property string logoUrl
                    required property bool installed
                    readonly property bool selected: root.selectedRow === index

                    width: results.width - Theme.space.md
                    height: 72
                    hoverEnabled: true
                    onClicked: root.select(index, { title: title, author: author, description: description, logoUrl: logoUrl, installed: installed })

                    background: Rectangle {
                        radius: Theme.radius.md
                        color: row.selected ? Theme.palette.accentSubtle
                             : row.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                        border.width: 1
                        border.color: row.selected ? Theme.palette.accent : Theme.palette.border
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.md
                        Rectangle {
                            Layout.leftMargin: Theme.space.sm
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: row.logoUrl
                                sourceSize: Qt.size(96, 96)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Row {
                                width: parent.width
                                spacing: Theme.space.sm
                                Text {
                                    id: rowTitle
                                    width: Math.min(implicitWidth, parent.width - (installedBadge.visible ? installedBadge.width + parent.spacing : 0))
                                    text: row.title
                                    elide: Text.ElideRight
                                    color: Theme.palette.textPrimary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.body.pixelSize
                                    font.weight: Font.DemiBold
                                }
                                StatusBadge {
                                    id: installedBadge
                                    anchors.verticalCenter: rowTitle.verticalCenter
                                    visible: row.installed
                                    tone: "success"
                                    text: qsTr("Installed")
                                }
                            }
                            Text {
                                width: parent.width
                                text: row.description
                                elide: Text.ElideRight
                                color: Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.caption.pixelSize
                            }
                        }
                    }
                }

                footer: Item {
                    width: results.width
                    height: root.browser && root.browser.searching ? 64 : Theme.space.md
                    BusyIndicator {
                        anchors.centerIn: parent
                        running: !!root.browser && root.browser.searching
                        visible: running
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    visible: root.searched && !!root.browser && !root.browser.searching && results.count === 0
                    title: root.browser && root.browser.error.length > 0 ? qsTr("Search failed") : qsTr("Nothing found")
                    body: root.browser && root.browser.error.length > 0 ? root.browser.error
                                                                         : qsTr("Try other words, or the other platform.")
                    MeshIcon { iconName: "search"; size: 40; color: Theme.palette.textTertiary }
                }
            }

            // Selected project
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.maximumWidth: 420
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space.lg
                    spacing: Theme.space.md
                    visible: !!root.selectedProject

                    Text {
                        Layout.fillWidth: true
                        text: root.selectedProject ? root.selectedProject.title : ""
                        wrapMode: Text.Wrap
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.heading.pixelSize
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.selectedProject && root.selectedProject.author ? qsTr("by %1").arg(root.selectedProject.author) : ""
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.selectedProject ? root.selectedProject.description : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 4
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }

                    Text {
                        text: qsTr("Versions")
                        color: Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.title.pixelSize
                        font.weight: Font.Bold
                    }

                    ListView {
                        id: versionList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: root.versions
                        ScrollBar.vertical: ScrollBar {}

                        delegate: AbstractButton {
                            id: versionRow
                            required property int index
                            required property var modelData
                            readonly property bool picked: root.pickedVersion === index

                            width: versionList.width - Theme.space.sm
                            height: 44
                            hoverEnabled: true
                            enabled: !root.installing
                            onClicked: root.pickedVersion = index

                            background: Rectangle {
                                radius: Theme.radius.md
                                color: versionRow.picked ? Theme.palette.accentSubtle
                                     : versionRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                            }
                            contentItem: Column {
                                leftPadding: Theme.space.sm
                                spacing: 1
                                Text {
                                    width: versionRow.width - Theme.space.md
                                    text: versionRow.modelData.versionNumber || versionRow.modelData.name
                                    elide: Text.ElideRight
                                    color: versionRow.modelData.isCompatible ? Theme.palette.textPrimary : Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                    font.weight: versionRow.picked ? Font.Bold : Font.Medium
                                }
                                Text {
                                    width: versionRow.width - Theme.space.md
                                    text: [ (versionRow.modelData.gameVersions || []).slice(0, 3).join(", "),
                                            (versionRow.modelData.loaders || []).join(", "),
                                            versionRow.modelData.isCompatible ? "" : qsTr("not for this instance") ]
                                          .filter(t => t.length > 0).join("  ·  ")
                                    elide: Text.ElideRight
                                    color: versionRow.modelData.isCompatible ? Theme.palette.textTertiary : Theme.palette.warning
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.caption.pixelSize
                                }
                            }
                        }

                        BusyIndicator {
                            anchors.centerIn: parent
                            running: !!root.browser && root.browser.versionsLoading
                            visible: running
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.watcher && root.watcher.failed ? (root.watcher.error || qsTr("Install failed."))
                            : root.watcher && root.watcher.succeeded ? qsTr("Installed.")
                            : root.watcher ? (root.watcher.status || "") : ""
                        wrapMode: Text.Wrap
                        color: root.watcher && root.watcher.failed ? Theme.palette.danger
                             : root.watcher && root.watcher.succeeded ? Theme.palette.success : Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    LaunchProgressBar {
                        Layout.fillWidth: true
                        visible: root.installing
                        progress: root.watcher ? root.watcher.progress : -1
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: !root.installing && root.pickedVersion >= 0 && !!root.installer
                        text: root.installing ? qsTr("Installing…")
                            : root.selectedProject && root.selectedProject.installed ? qsTr("Install this version")
                            : qsTr("Install")
                        icon.source: Icons.url("download")
                        onClicked: root.watcher = root.installer(root.selectedRow, root.versions[root.pickedVersion].id)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - Theme.space.xxl * 2
                    horizontalAlignment: Text.AlignHCenter
                    visible: !root.selectedProject
                    text: qsTr("Pick something on the left to see its versions and install it. Required dependencies come along automatically.")
                    wrapMode: Text.Wrap
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }
    }
}
