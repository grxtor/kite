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
 * the newest compatible one preselected. Installs run side by side: each
 * project keeps its own progress, shown on its row, so browsing on and
 * installing something else never waits for the previous download.
 */
Item {
    id: root

    // ContentBrowser of the open instance.
    property var browser: null
    // (row, versionId) -> TaskWatcher, C++-owned.
    property var installer: null
    property int selectedRow: -1
    property var selectedProject: null
    property int pickedVersion: -1

    readonly property var versions: browser && browser.versions ? browser.versions : []

    // Every install started from here, by project key. A plain JS object
    // does not notify, so `downloadsRevision` is bumped on every change and
    // read by whatever looks a download up.
    property var downloads: ({})
    property int downloadsRevision: 0
    readonly property int activeDownloads: {
        root.downloadsRevision
        var n = 0
        for (var key in root.downloads) {
            if (root.downloads[key] && root.downloads[key].running)
                ++n
        }
        return n
    }

    function projectKey(title, author) {
        var provider = root.browser ? root.browser.provider : ""
        var type = root.browser ? root.browser.contentType : ""
        return provider + "/" + type + "/" + title + "/" + author
    }
    function downloadFor(key) {
        root.downloadsRevision
        return root.downloads[key] || null
    }
    readonly property var selectedDownload: selectedProject
                                            ? downloadFor(projectKey(selectedProject.title, selectedProject.author)) : null
    readonly property bool selectedInstalling: !!selectedDownload && selectedDownload.running

    function install() {
        if (!root.installer || root.pickedVersion < 0 || !root.selectedProject)
            return
        var watcher = root.installer(root.selectedRow, root.versions[root.pickedVersion].id)
        if (!watcher)
            return
        root.downloads[projectKey(root.selectedProject.title, root.selectedProject.author)] = watcher
        watcher.runningChanged.connect(function () { root.downloadsRevision++ })
        root.downloadsRevision++
    }

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
        root.pickedVersion = -1
        descriptionText.expanded = false
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

        // What to look for, and where.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            SegmentedControl {
                options: root.types
                current: root.browser ? root.browser.contentType : "mods"
                onActivated: (value) => { root.browser.contentType = value; root.runSearch() }
            }
            Item { Layout.fillWidth: true }
            // Installs in flight, so a download started on another project
            // is still visible after moving on.
            Rectangle {
                visible: root.activeDownloads > 0
                implicitWidth: downloadsRow.implicitWidth + Theme.space.md * 2
                implicitHeight: Theme.control.heightSm
                radius: Theme.radius.pill
                color: Theme.palette.accentSubtle
                Row {
                    id: downloadsRow
                    anchors.centerIn: parent
                    spacing: Theme.space.xs
                    BusyIndicator {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.icon.sm
                        height: Theme.icon.sm
                        running: parent.visible
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("%n installing", "", root.activeDownloads)
                        color: Theme.palette.accentText
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        font.weight: Font.DemiBold
                    }
                }
            }
            SegmentedControl {
                options: root.providers
                current: root.browser ? root.browser.provider : "modrinth"
                onActivated: (value) => { root.browser.provider = value; root.runSearch() }
            }
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
                Layout.minimumWidth: 0
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
                    readonly property var download: root.downloadFor(root.projectKey(title, author))
                    readonly property bool downloading: !!download && download.running
                    readonly property bool justInstalled: !!download && download.succeeded

                    width: results.width - Theme.space.md
                    height: 60
                    hoverEnabled: true
                    onClicked: root.select(index, { title: title, author: author, description: description, logoUrl: logoUrl, installed: installed })

                    background: Rectangle {
                        radius: Theme.radius.md
                        color: row.selected ? Theme.palette.accentSubtle
                             : row.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
                        border.width: 1
                        border.color: row.selected ? Theme.palette.accent : Theme.palette.border
                        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                        // The download's progress along the row's foot.
                        LaunchProgressBar {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Theme.space.xs
                            height: 3
                            visible: row.downloading
                            progress: row.download ? row.download.progress : -1
                        }
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.md
                        Rectangle {
                            Layout.leftMargin: Theme.space.sm
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            clip: true
                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: row.logoUrl
                                sourceSize: Qt.size(80, 80)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2
                            Text {
                                width: parent.width
                                text: row.title
                                elide: Text.ElideRight
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                font.weight: Font.DemiBold
                            }
                            Text {
                                width: parent.width
                                text: row.downloading ? (row.download.status || qsTr("Installing…")) : row.description
                                elide: Text.ElideRight
                                color: row.downloading ? Theme.palette.accentText : Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.caption.pixelSize
                            }
                        }
                        StatusBadge {
                            Layout.rightMargin: Theme.space.sm
                            visible: row.installed || row.justInstalled || (!!row.download && row.download.failed)
                            tone: row.download && row.download.failed ? "danger" : "success"
                            text: row.download && row.download.failed ? qsTr("Failed") : qsTr("Installed")
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

            // Selected project: a compact header, then the versions, which
            // get every pixel the header does not need.
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                Layout.minimumWidth: 300
                Layout.maximumWidth: 440
                radius: Theme.radius.lg
                color: Theme.palette.surface
                border.width: 1
                border.color: Theme.palette.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space.lg
                    spacing: Theme.space.md
                    visible: !!root.selectedProject

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.md
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            Image {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: root.selectedProject ? root.selectedProject.logoUrl : ""
                                sourceSize: Qt.size(96, 96)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2
                            Text {
                                width: parent.width
                                text: root.selectedProject ? root.selectedProject.title : ""
                                elide: Text.ElideRight
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.title.pixelSize + 2
                                font.weight: Font.Bold
                            }
                            Text {
                                width: parent.width
                                visible: text.length > 0
                                text: root.selectedProject && root.selectedProject.author ? qsTr("by %1").arg(root.selectedProject.author) : ""
                                elide: Text.ElideRight
                                color: Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }
                        }
                    }

                    // Three lines, the rest on request: the version list is
                    // what this panel is for.
                    Text {
                        id: descriptionText
                        property bool expanded: false
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.selectedProject ? root.selectedProject.description : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: expanded ? 12 : 3
                        elide: Text.ElideRight
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        lineHeight: 1.25
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
                            font.weight: Font.Bold
                        }
                        Text {
                            visible: root.versions.length > 0
                            text: qsTr("%n available", "", root.versions.length)
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 160
                        radius: Theme.radius.md
                        color: Theme.palette.surfaceSunken
                        border.width: 1
                        border.color: Theme.palette.border

                        ListView {
                            id: versionList
                            anchors.fill: parent
                            anchors.margins: Theme.space.xs
                            clip: true
                            spacing: 2
                            model: root.versions
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}

                            delegate: AbstractButton {
                                id: versionRow
                                required property int index
                                required property var modelData
                                readonly property bool picked: root.pickedVersion === index

                                width: versionList.width - Theme.space.sm
                                height: 40
                                hoverEnabled: true
                                onClicked: root.pickedVersion = index

                                background: Rectangle {
                                    radius: Theme.radius.sm + 2
                                    color: versionRow.picked ? Theme.palette.accentSubtle
                                         : versionRow.hovered ? Theme.palette.hoverOverlay : "transparent"
                                    border.width: versionRow.picked ? 1 : 0
                                    border.color: Theme.palette.accent
                                }
                                contentItem: RowLayout {
                                    spacing: Theme.space.sm
                                    Column {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.leftMargin: Theme.space.sm
                                        spacing: 0
                                        Text {
                                            width: parent.width
                                            text: versionRow.modelData.versionNumber || versionRow.modelData.name
                                            elide: Text.ElideRight
                                            color: versionRow.modelData.isCompatible ? Theme.palette.textPrimary : Theme.palette.textTertiary
                                            font.family: Theme.font.family
                                            font.pixelSize: Theme.type.label.pixelSize
                                            font.weight: versionRow.picked ? Font.Bold : Font.Medium
                                        }
                                        Text {
                                            width: parent.width
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
                                    MeshIcon {
                                        Layout.rightMargin: Theme.space.sm
                                        visible: versionRow.picked
                                        iconName: "check"
                                        size: Theme.icon.sm
                                        color: Theme.palette.accentText
                                    }
                                }
                            }

                            BusyIndicator {
                                anchors.centerIn: parent
                                running: !!root.browser && root.browser.versionsLoading
                                visible: running
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        readonly property var d: root.selectedDownload
                        text: d && d.failed ? (d.error || qsTr("Install failed."))
                            : d && d.succeeded ? qsTr("Installed.")
                            : d ? (d.status || "") : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: d && d.failed ? Theme.palette.danger
                             : d && d.succeeded ? Theme.palette.success : Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    LaunchProgressBar {
                        Layout.fillWidth: true
                        visible: root.selectedInstalling
                        progress: root.selectedDownload ? root.selectedDownload.progress : -1
                    }

                    Button {
                        Layout.fillWidth: true
                        highlighted: true
                        enabled: !root.selectedInstalling && root.pickedVersion >= 0 && !!root.installer
                        text: root.selectedInstalling ? qsTr("Installing…")
                            : root.selectedProject && root.selectedProject.installed ? qsTr("Install this version")
                            : qsTr("Install")
                        icon.source: Icons.url("download")
                        onClicked: root.install()
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
