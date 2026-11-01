// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Find mods, resource packs, shaders and data packs on Modrinth or CurseForge and install them, dependencies
 * included. Results start out matching the instance's Minecraft version and loader. Installs run side by
 * side: each project keeps its own progress.
 */
Item {
    id: root

    property var browser: null
    // InstanceDetails, for its loaderInstaller; without it the "Install a loader" notice has no button.
    property var details: null
    signal backRequested()
    property int selectedRow: -1
    property var selectedProject: null
    property int pickedVersion: -1

    readonly property var versions: browser && browser.versions ? browser.versions : []
    readonly property int providerWidth: 150
    readonly property int sortWidth: 190
    readonly property int searchMinWidth: 160
    readonly property real panelShare: 0.4
    readonly property int panelMinWidth: 300
    readonly property int panelMaxWidth: 440
    readonly property int busyFooterHeight: 64

    // Every install started here, by project key. A plain JS object does not notify, so downloadsRevision is bumped
    // on every change.
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

    // Reviews still working out dependencies, by project key; same revision counter as `downloads`.
    property var checking: ({})
    function isChecking(key) {
        root.downloadsRevision
        return !!root.checking[key]
    }
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
    function openFilters() {
        filterPopup.open()
    }
    // Review snapshots: "browsepick" picks the first result once the search has answered.
    function review(name) {
        if (name === "browsepick")
            reviewPick.restart()
    }
    Timer {
        id: reviewPick
        interval: Theme.motion.slow
        repeat: true
        onTriggered: {
            const first = results.itemAtIndex(0)
            if (first) {
                stop()
                first.clicked()
            }
        }
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
        root.browser.loadVersions(row)
    }
    function closeProject() {
        root.installNote = ""
        root.selectedRow = -1
        root.selectedProject = null
    }
    // Opens on @p kind when it can be browsed; searched again once shown.
    function showKind(kind) {
        if (!root.browser || !root.types.some((t) => t.value === kind) || root.browser.contentType === kind)
            return
        root.browser.contentType = kind
        root.closeProject()
        root.searched = false
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
            Layout.preferredHeight: Theme.control.height
            spacing: Theme.space.lg

            Copy {
                text: qsTr("Add content")
                role: "heading"
                Accessible.role: Accessible.Heading
            }
            ChipGroup {
                options: root.types
                current: root.browser ? root.browser.contentType : "mods"
                onActivated: (value) => {
                    root.browser.contentType = value
                    root.runSearch()
                }
            }
            Item { Layout.fillWidth: true }
            BoxSurface {
                visible: root.activeDownloads > 0
                implicitWidth: downloadsRow.implicitWidth + Theme.space.md * 2
                implicitHeight: Theme.control.heightSm
                radius: Theme.radius.sm
                color: Theme.palette.surfaceRaised
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
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("%n installing", "", root.activeDownloads)
                        role: "label"
                        font.weight: Font.DemiBold
                    }
                }
            }
        }

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
            Layout.fillWidth: true
            spacing: Theme.space.md

            ComboBox {
                id: providerBox
                Layout.preferredWidth: root.providerWidth
                model: root.providers
                textRole: "label"
                valueRole: "value"
                currentIndex: root.providers.findIndex((p) => p.value === (root.browser ? root.browser.provider : "modrinth"))
                Accessible.name: qsTr("Search on")
                onActivated: {
                    root.browser.provider = currentValue
                    currentIndex = Qt.binding(() => root.providers.findIndex((p) => p.value === (root.browser ? root.browser.provider : "modrinth")))
                    root.runSearch()
                }
            }
            SearchBox {
                id: searchField
                Layout.fillWidth: true
                Layout.minimumWidth: root.searchMinWidth
                placeholderText: qsTr("Search %1").arg(providerBox.currentText)
                onTextEdited: debounce.restart()
                onTextChanged: if (text.length === 0) debounce.restart()
            }
            ComboBox {
                Layout.preferredWidth: root.sortWidth
                visible: count > 0
                model: root.browser && root.browser.sortOptions ? root.browser.sortOptions : []
                textRole: "label"
                valueRole: "id"
                Accessible.name: qsTr("Sort by")
                onActivated: { root.browser.sortIndex = currentValue; root.runSearch() }
            }
            FiltersButton {
                id: filtersButton
                count: root.browser ? root.browser.activeFilterCount : 0
                onClicked: filterPopup.opened ? filterPopup.close() : filterPopup.open()

                ContentFilterPopup {
                    id: filterPopup
                    browser: root.browser
                    y: filtersButton.height + Theme.space.xs
                    x: filtersButton.width - width
                }
            }
        }

        RowLayout {
            id: panes
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.lg

            Item {
                id: resultsArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                BoxSurface {
                    id: resultsCard
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: Math.min(parent.height, results.contentHeight + results.topMargin + results.bottomMargin + border.width * 2)
                    visible: results.count > 0 || (!!root.browser && root.browser.searching)
                    clip: true

                    ListView {
                        id: results
                        anchors.fill: parent
                        anchors.margins: resultsCard.border.width
                        topMargin: Theme.space.xs
                        bottomMargin: Theme.space.xs
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        model: root.browser ? root.browser.results : null
                        ScrollBar.vertical: ScrollBar {}
                        onAtYEndChanged: if (atYEnd && root.browser && root.browser.canFetchMore && !root.browser.searching)
                                             root.browser.fetchMore()

                        delegate: ContentResultRow {
                            owner: root
                            width: ListView.view.width
                            lastRow: index === ListView.view.count - 1
                        }

                        footer: Item {
                            width: results.width
                            height: root.browser && root.browser.searching ? root.busyFooterHeight : 0
                            BusyIndicator {
                                anchors.centerIn: parent
                                running: !!root.browser && root.browser.searching
                                visible: running
                            }
                        }
                    }

                    // On the card, not the scrolling rows, so it stays on the card's bottom edge.
                    ScrollFade {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: resultsCard.border.width
                        flickable: results
                        fadeTo: Theme.palette.surface
                        // Rounded like the card, or its square foot covers the card's bottom corners.
                        radius: Theme.radius.lg
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: root.searched && !!root.browser && !root.browser.searching && results.count === 0
                    title: root.browser && root.browser.error.length > 0 ? qsTr("Search failed") : qsTr("Nothing found")
                    body: root.browser && root.browser.error.length > 0 ? root.browser.error
                                                                         : qsTr("Try other words, fewer filters, or the other site.")
                    MeshIcon { iconName: "search"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
                }
            }

            Item {
                id: panelSlot
                readonly property int fullWidth: Math.round(Math.min(root.panelMaxWidth,
                                                                     Math.max(root.panelMinWidth, panes.width * root.panelShare)))
                Layout.fillHeight: true
                Layout.preferredWidth: root.selectedProject ? fullWidth : 0
                visible: Layout.preferredWidth > 0
                clip: true
                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                }

                // Full width throughout, pinned to the slot's right edge: the slot's growth reveals it instead of
                // squeezing its text.
                ContentProjectPanel {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    width: panelSlot.fullWidth
                    owner: root
                    visible: !!root.selectedProject
                    onCloseRequested: root.closeProject()
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.visible && !filterPopup.opened && !reviewDialog.visible && !searchField.activeFocus
        onActivated: root.selectedProject ? root.closeProject() : root.backRequested()
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
