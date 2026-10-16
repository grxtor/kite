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
 * without leaving the instance. Results start out matching the instance's
 * Minecraft version and loader; Filters (ContentFilterPopup) widens or
 * narrows that, and adds categories and environment. Things already
 * installed say so.
 *
 * Results on the left, the picked project on the right with its versions,
 * the newest compatible one preselected. Install first works out what the
 * project needs; when that is anything at all, InstallReviewDialog lists
 * it to untick before downloading (DownloadSummaryDialog's job). Installs
 * run side by side: each project keeps its own progress, shown on its row,
 * so browsing on and installing something else never waits for the
 * previous download.
 */
Item {
    id: root

    // ContentBrowser of the open instance.
    property var browser: null
    // InstanceDetails, for its loaderInstaller (the "Install a loader"
    // action). Optional: without it that notice just has no button.
    property var details: null
    property int selectedRow: -1
    property var selectedProject: null
    property int pickedVersion: -1

    readonly property var versions: browser && browser.versions ? browser.versions : []
    // The sort box beside the search field.
    readonly property int sortWidth: 200
    // The search field is not worth having narrower than this; below it,
    // it takes a row of its own above the sort box and Filters.
    readonly property int searchMinWidth: 200
    // The project panel's share of the row, between its own bounds, so on a
    // narrow window the results keep room for their names.
    readonly property real panelShare: 0.4
    readonly property int panelMinWidth: 300
    readonly property int panelMaxWidth: 440
    // A result row, its logo, the selected project's logo, and the least
    // the version list shrinks to under the project's header.
    readonly property int resultHeight: 60
    readonly property int resultLogoSize: 40
    readonly property int projectLogoSize: 48
    readonly property int versionListMinHeight: 160

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
    readonly property bool selectedChecking: selectedProject
                                             ? root.isChecking(projectKey(selectedProject.title, selectedProject.author)) : false

    // Reviews still working out dependencies, by project key, under the
    // same revision counter as `downloads`.
    property var checking: ({})
    function isChecking(key) {
        root.downloadsRevision
        return !!root.checking[key]
    }
    // Why the install button is off, if it is.
    readonly property string blockedReason: !root.browser ? ""
        : !root.browser.changesAllowed ? qsTr("The game is running. Mods can be installed once it closes.")
        : !root.browser.installTargetAvailable ? qsTr("This instance's shared data pack folder is turned off.")
        : ""
    property string installNote: ""

    function install() {
        if (!root.browser || root.pickedVersion < 0 || !root.selectedProject)
            return
        root.installNote = ""
        var key = projectKey(root.selectedProject.title, root.selectedProject.author)
        var review = root.browser.review(root.selectedRow, root.versions[root.pickedVersion].id)
        if (!review) {
            root.installNote = qsTr("This version cannot be installed here.")
            return
        }
        root.checking[key] = review
        root.downloadsRevision++
        var settle = function () {
            if (!review.ready || root.checking[key] !== review)
                return
            delete root.checking[key]
            root.downloadsRevision++
            if (review.needsReview)
                root.queueReview(key, review)
            else
                root.track(key, review.install())
        }
        review.stateChanged.connect(settle)
        settle()
    }
    function track(key, watcher) {
        if (!watcher)
            return
        root.downloads[key] = watcher
        watcher.runningChanged.connect(function () { root.downloadsRevision++ })
        root.downloadsRevision++
    }

    // Reviews waiting for the dialog, oldest first.
    property var reviewQueue: []
    function queueReview(key, review) {
        root.reviewQueue = root.reviewQueue.concat([{ key: key, review: review }])
        root.showNextReview()
    }
    function showNextReview() {
        if (reviewDialog.visible || root.reviewQueue.length === 0)
            return
        var next = root.reviewQueue[0]
        root.reviewQueue = root.reviewQueue.slice(1)
        reviewDialog.key = next.key
        reviewDialog.show(next.review)
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
    // The Filters popup, open: for review snapshots (Main.qml's
    // "browsefilters" dev route, through InstancePage).
    function openFilters() {
        filterPopup.open()
    }

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
        root.installNote = ""
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
                // A status chip, not a toggle: control corners, not a pill.
                radius: Theme.radius.sm
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

        // At most one: the game running, no loader for mods, or nowhere for
        // data packs to go.
        NoticeBar {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.blockedReason.length > 0 ? root.blockedReason
                : root.browser && root.browser.contentType === "mods" && !root.browser.hasModLoader
                  ? qsTr("This instance has no mod loader, so mods will not run. Install one first.")
                : ""
            actionText: !!root.details && root.blockedReason.length === 0 && text.length > 0
                        ? qsTr("Install a loader…") : ""
            onActionTriggered: loaderDialog.open()
        }

        RowLayout {
            id: panes
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.lg

            // Results, with the search that drives them over them: the
            // field spans exactly the list, and its top lines up with the
            // project panel's.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                Layout.minimumWidth: 0
                spacing: Theme.space.md

                GridLayout {
                    id: searchRow
                    // Search, sort and Filters on one row while they fit;
                    // otherwise the search gets the full width above them.
                    readonly property bool stacked: width < root.searchMinWidth + root.sortWidth
                                                    + filtersButton.implicitWidth + columnSpacing * 2
                    Layout.fillWidth: true
                    columns: stacked ? 2 : 3
                    columnSpacing: Theme.space.md
                    rowSpacing: Theme.space.md

                    SearchBox {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.columnSpan: searchRow.stacked ? 2 : 1
                        placeholderText: qsTr("Search")
                        onTextEdited: debounce.restart()
                        onTextChanged: if (text.length === 0) debounce.restart()
                    }
                    ComboBox {
                        Layout.preferredWidth: root.sortWidth
                        Layout.fillWidth: searchRow.stacked
                        visible: count > 0
                        model: root.browser && root.browser.sortOptions ? root.browser.sortOptions : []
                        textRole: "label"
                        valueRole: "id"
                        onActivated: { root.browser.sortIndex = currentValue; root.runSearch() }
                    }
                    Button {
                        id: filtersButton
                        readonly property int active: root.browser ? root.browser.activeFilterCount : 0
                        text: active > 0 ? qsTr("Filters (%1)").arg(active) : qsTr("Filters")
                        icon.source: Icons.url("filter")
                        onClicked: filterPopup.opened ? filterPopup.close() : filterPopup.open()

                        ContentFilterPopup {
                            id: filterPopup
                            browser: root.browser
                            y: filtersButton.height + Theme.space.xs
                            x: filtersButton.width - width
                        }
                    }
                }

                ListView {
                    id: results
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    // Parented to the view itself, not its scrolling
                    // content, so it stays on the bottom edge.
                    ScrollFade {
                        parent: results
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        flickable: results
                    }
                    // The same row rhythm as every list tab (see PackList).
                    spacing: Theme.space.sm
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
                        readonly property bool checking: root.isChecking(root.projectKey(title, author))
                        readonly property bool justInstalled: !!download && download.succeeded

                        // Full width: the ScrollBar overlays the rows' right
                        // edge rather than reserving a gutter beside them.
                        width: results.width
                        height: root.resultHeight
                        // The icon sits as far in as on Content's rows, and
                        // the text stops as far from the right border.
                        leftPadding: Theme.space.md
                        rightPadding: Theme.space.md
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
                                visible: row.downloading || row.checking
                                progress: row.downloading && row.download ? row.download.progress : -1
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Theme.space.md
                            Rectangle {
                                Layout.preferredWidth: root.resultLogoSize
                                Layout.preferredHeight: root.resultLogoSize
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
                                    text: row.downloading ? (row.download.status || qsTr("Installing…"))
                                        : row.checking ? qsTr("Checking what it needs…") : row.description
                                    elide: Text.ElideRight
                                    color: row.downloading || row.checking ? Theme.palette.accentText : Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.caption.pixelSize
                                }
                            }
                            StatusBadge {
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
            }

            // Selected project: a compact header, then the versions, which
            // get every pixel the header does not need.
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: Math.round(Math.min(root.panelMaxWidth,
                                                           Math.max(root.panelMinWidth, panes.width * root.panelShare)))
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
                            Layout.preferredWidth: root.projectLogoSize
                            Layout.preferredHeight: root.projectLogoSize
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
                        Layout.minimumHeight: root.versionListMinHeight
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

                            delegate: VersionRow {
                                required property int index
                                required property var modelData
                                width: versionList.width - Theme.space.sm
                                entry: modelData
                                picked: root.pickedVersion === index
                                onClicked: root.pickedVersion = index
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
                        text: root.installNote.length > 0 ? root.installNote
                            : root.selectedChecking ? qsTr("Checking what it needs…")
                            : d && d.failed ? (d.error || qsTr("Install failed."))
                            : d && d.succeeded ? qsTr("Installed.")
                            : d ? (d.status || "") : ""
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: root.installNote.length > 0 || (d && d.failed) ? Theme.palette.danger
                             : d && d.succeeded && !root.selectedChecking ? Theme.palette.success : Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    LaunchProgressBar {
                        Layout.fillWidth: true
                        visible: root.selectedInstalling
                        progress: root.selectedDownload ? root.selectedDownload.progress : -1
                    }

                    HoverReason {
                        Layout.fillWidth: true
                        reason: root.blockedReason
                        Button {
                            width: parent ? parent.width : implicitWidth
                            highlighted: true
                            enabled: !root.selectedInstalling && !root.selectedChecking && root.pickedVersion >= 0
                                     && !!root.browser && root.blockedReason.length === 0
                            text: root.selectedInstalling ? qsTr("Installing…")
                                : root.selectedChecking ? qsTr("Checking…")
                                : root.selectedProject && root.selectedProject.installed ? qsTr("Install this version")
                                : qsTr("Install")
                            icon.source: Icons.url("download")
                            onClicked: root.install()
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - Theme.space.xxl * 2
                    horizontalAlignment: Text.AlignHCenter
                    visible: !root.selectedProject
                    text: qsTr("Pick something on the left to see its versions and install it. Anything it needs is listed before it downloads.")
                    wrapMode: Text.Wrap
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }
    }

    InstallReviewDialog {
        id: reviewDialog
        property string key: ""
        onInstallStarted: (watcher) => root.track(reviewDialog.key, watcher)
        onClosed: Qt.callLater(root.showNextReview)
    }

    LoaderInstallDialog {
        id: loaderDialog
        installer: root.details ? root.details.loaderInstaller : null
    }
}
