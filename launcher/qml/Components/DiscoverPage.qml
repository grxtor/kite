// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Browse modpacks and install them as new instances: a provider drop-down (Modrinth, CurseForge, FTB,
 * ATLauncher, Technic) over a toolbar and a table of ModpackCard rows. The provider models share
 * properties, invokables and row roles, so everything works on `active`. Filters a provider cannot
 * answer are left out.
 */
Item {
    id: root

    property var model: null
    property var installer: null
    // The other providers' models; null ones are not offered.
    property var curseForgeModel: null
    property var ftbModel: null
    property var atlModel: null
    property var technicModel: null
    property string provider: "modrinth"

    readonly property var providers: [
        { value: "modrinth", label: "Modrinth", model: root.model },
        { value: "curseforge", label: "CurseForge", model: root.curseForgeModel },
        { value: "ftb", label: "FTB", model: root.ftbModel },
        { value: "atlauncher", label: "ATLauncher", model: root.atlModel },
        { value: "technic", label: "Technic", model: root.technicModel }
    ].filter((p) => !!p.model)
    readonly property var current: root.providers.find((p) => p.value === root.provider) || root.providers[0] || null
    readonly property var active: root.current ? root.current.model : null
    readonly property string providerName: root.current ? root.current.label : "Modrinth"
    readonly property bool keyMissing: !!root.active && root.active.keyMissing === true
    // ATLauncher and Technic say nothing about loaders; Technic has no sorts.
    readonly property bool loaderFilterAvailable: root.provider !== "atlauncher" && root.provider !== "technic"
    // The loader is the only filter here.
    readonly property int activeFilterCount: !!root.active && root.active.loader.length > 0 ? 1 : 0
    // Modrinth installs through the shell; the other models install themselves (C++ owns their TaskWatchers).
    readonly property var activeInstaller: root.provider === "modrinth"
        ? root.installer
        : function (projectId, versionId, name, group) { return root.active.install(projectId, versionId, name, group) }
    signal showInstanceRequested(string id)
    signal otherPlatformsRequested()

    property var openedPack: null

    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int emptyStateWidth: 400
    readonly property int contentWidth: Math.max(0, Math.min(width - pagePadding * 2, Theme.layout.maxContentWidth))
    readonly property int contentX: Math.max(pagePadding, Math.round((width - contentWidth) / 2))

    // On a narrow page the boxes step down first so the search field keeps room for its placeholder.
    readonly property bool narrowToolbar: root.contentWidth < 820
    readonly property int providerWidth: root.narrowToolbar ? 124 : 150
    readonly property int sortWidth: root.narrowToolbar ? 164 : 190
    readonly property int searchMinWidth: 160
    readonly property int filterPanelWidth: 240
    readonly property int skeletonRows: 6

    readonly property var loaders: [
        { value: "", label: qsTr("Any loader") },
        { value: "fabric", label: "Fabric" },
        { value: "forge", label: "Forge" },
        { value: "neoforge", label: "NeoForge" },
        { value: "quilt", label: "Quilt" }
    ]

    function openResult(index) {
        var item = resultsRepeater.itemAt(index)
        if (item)
            item.clicked()
    }

    function open(pack) {
        root.openedPack = pack
        if (root.active)
            root.active.loadDetail(pack.projectId)
    }

    // The providers a search has been asked of, so "nothing found" never shows for a search that has not
    // happened. Replaced, never changed in place, so bindings see it.
    property var searchedProviders: []
    readonly property bool searched: root.searchedProviders.indexOf(root.provider) >= 0

    function runSearch() {
        if (!root.active)
            return
        if (!root.searched)
            root.searchedProviders = root.searchedProviders.concat([root.provider])
        root.active.search()
    }

    // Carries the search words and loader across; searches only if the other provider's results do not answer them.
    function setProvider(value) {
        if (value === root.provider)
            return
        var query = searchField.text
        var loader = root.active ? root.active.loader : ""
        root.openedPack = null
        root.provider = value
        if (!root.active)
            return
        var stale = root.active.query !== query || root.active.loader !== loader
        root.active.query = query
        root.active.loader = loader
        if (!root.searched || stale)
            root.runSearch()
    }

    // Main.qml binds this to its PageSlot's StackLayout.isCurrentItem: the page sits in a Loader, so its own
    // attached one never turns true. Not `visible`, which can still be true at Component.onCompleted.
    property bool shown: false

    // Searches on first show, not at startup, so opening the launcher never talks to Modrinth on its own.
    function searchIfFirstShown() {
        if (root.shown && !root.searched)
            runSearch()
    }
    onShownChanged: searchIfFirstShown()
    onModelChanged: searchIfFirstShown()
    Component.onCompleted: searchIfFirstShown()

    Timer {
        id: debounce
        interval: 350
        onTriggered: {
            if (!root.active)
                return
            root.active.query = searchField.text
            root.runSearch()
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.openedPack ? 1 : 0

        ColumnLayout {
            spacing: Theme.space.lg

            RowLayout {
                id: toolbar
                Layout.topMargin: Theme.space.xl
                Layout.leftMargin: root.contentX
                Layout.preferredWidth: root.contentWidth
                Layout.maximumWidth: root.contentWidth
                spacing: Theme.space.md

                ComboBox {
                    id: providerBox
                    Layout.preferredWidth: root.providerWidth
                    visible: root.providers.length > 1
                    model: root.providers.map((p) => ({ value: p.value, label: p.label }))
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.providers.findIndex((p) => p.value === root.provider)
                    Accessible.name: qsTr("Browse modpacks on")
                    onActivated: {
                        root.setProvider(currentValue)
                        currentIndex = Qt.binding(() => root.providers.findIndex((p) => p.value === root.provider))
                    }
                }

                SearchBox {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: root.searchMinWidth
                    placeholderText: qsTr("Search modpacks on %1").arg(root.providerName)
                    onTextEdited: debounce.restart()
                    onTextChanged: if (text.length === 0) debounce.restart()
                }

                ComboBox {
                    id: sortBox
                    Layout.preferredWidth: root.sortWidth
                    visible: count > 1
                    model: root.active && root.active.sortOptions ? root.active.sortOptions : []
                    textRole: "label"
                    valueRole: "id"
                    Accessible.name: qsTr("Sort modpacks by")
                    // Re-read once the box has the new provider's entries; indexOfValue() needs them.
                    function syncSort() {
                        currentIndex = root.active ? Math.max(0, indexOfValue(root.active.sort)) : 0
                    }
                    onCountChanged: syncSort()
                    Component.onCompleted: syncSort()
                    Connections {
                        target: root
                        function onActiveChanged() { Qt.callLater(sortBox.syncSort) }
                    }
                    onActivated: {
                        if (!root.active)
                            return
                        root.active.sort = currentValue
                        root.runSearch()
                    }
                }

                FiltersButton {
                    id: filtersButton
                    visible: root.loaderFilterAvailable
                    count: root.activeFilterCount
                    onClicked: filterPopup.opened ? filterPopup.close() : filterPopup.open()

                    Popup {
                        id: filterPopup
                        y: filtersButton.height + Theme.space.xs
                        x: filtersButton.width - width
                        width: root.filterPanelWidth
                        padding: Theme.space.lg
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                        contentItem: ColumnLayout {
                            spacing: Theme.space.xs

                            Copy {
                                Layout.fillWidth: true
                                Layout.bottomMargin: Theme.space.xs
                                text: qsTr("Loader")
                                role: "bodyStrong"
                            }
                            Repeater {
                                model: root.loaders
                                delegate: RadioButton {
                                    id: loaderRadio
                                    required property var modelData
                                    readonly property bool current: !!root.active && root.active.loader === modelData.value
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    // Checked follows the provider's loader; a click only asks, and restores the
                                    // binding it breaks.
                                    autoExclusive: false
                                    checked: loaderRadio.current
                                    onClicked: {
                                        if (!loaderRadio.current && root.active) {
                                            root.active.loader = modelData.value
                                            root.runSearch()
                                        }
                                        loaderRadio.checked = Qt.binding(() => loaderRadio.current)
                                    }
                                }
                            }
                        }
                    }
                }

                Button {
                    id: importButton
                    text: qsTr("Import")
                    icon.source: Icons.url("archive")
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Import a pack exported from CurseForge, FTB, ATLauncher or Technic")
                    onClicked: root.otherPlatformsRequested()
                }
            }

            Flickable {
                id: flick
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: content.y + content.height + Theme.space.xl
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: ScrollBar {}

                // QML views never ask for more on their own; ask as the
                // end comes into view.
                onAtYEndChanged: if (atYEnd && root.active && root.active.canFetchMore && !root.active.searching)
                                     root.active.fetchMore()

                readonly property int placeholderRows: root.active && root.active.searching
                                                       ? (root.active.count === 0 ? root.skeletonRows : 1) : 0

                Column {
                    id: content
                    x: root.contentX
                    width: root.contentWidth
                    spacing: Theme.space.md

                    Repeater {
                        id: resultsRepeater
                        model: root.active
                        delegate: ModpackCard {
                            width: content.width
                            projectId: model.projectId
                            title: model.title
                            author: model.author
                            description: model.description
                            logoUrl: model.logoUrl
                            downloads: model.downloads
                            updated: model.updated
                            categories: model.categories || []
                            // Not every provider has a cover (ATLauncher).
                            galleryUrl: model.galleryUrl || ""
                            accentColor: model.accentColor
                            onClicked: root.open({ projectId: projectId, title: title, author: author,
                                                   description: description, logoUrl: logoUrl,
                                                   downloads: downloads, updated: updated,
                                                   categories: model.categories, galleryUrl: galleryUrl,
                                                   accentColor: accentColor })
                        }
                    }

                    Repeater {
                        model: flick.placeholderRows
                        delegate: ModpackCard {
                            width: content.width
                            skeleton: true
                        }
                    }
                }
            }
        }

        ModpackDetail {
            pack: root.openedPack || ({})
            detail: root.active ? root.active.detail : null
            model: root.active
            installer: root.activeInstaller
            // CurseForge describes a pack in HTML, Modrinth in Markdown.
            bodyFormat: root.active && root.active.bodyFormat ? root.active.bodyFormat : "markdown"
            contentX: root.contentX
            contentWidth: root.contentWidth
            onBackRequested: root.openedPack = null
            onShowInstanceRequested: (id) => root.showInstanceRequested(id)
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !root.openedPack && flick.contentY + flick.height < flick.contentHeight - 1
        flickable: flick
    }

    EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - root.pagePadding * 2, root.emptyStateWidth)
        visible: root.searched && !root.keyMissing && !root.openedPack && !!root.active
                 && !root.active.searching && root.active.count === 0
        title: root.active && root.active.error.length > 0 ? qsTr("Couldn't reach %1").arg(root.providerName)
                                                           : qsTr("No modpacks found")
        body: root.active && root.active.error.length > 0 ? root.active.error
                                                          : qsTr("Try fewer words, or another loader.")
        actionText: root.active && root.active.error.length > 0 ? qsTr("Try again") : ""
        actionIcon: "refresh"
        onActionTriggered: root.runSearch()

        MeshIcon {
            iconName: root.active && root.active.error.length > 0 ? "alert-triangle" : "search"
            size: Theme.icon.xl
            color: Theme.palette.textTertiary
        }
    }

    // A build without a CurseForge API key cannot ask CurseForge anything.
    EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - root.pagePadding * 2, root.emptyStateWidth)
        visible: root.keyMissing && !root.openedPack
        title: qsTr("CurseForge isn't available")
        body: qsTr("This build has no CurseForge API key, so its modpacks can't be browsed here. Modrinth works as usual.")
        actionText: qsTr("Browse Modrinth")
        actionIcon: "compass"
        onActionTriggered: root.setProvider("modrinth")
        MeshIcon { iconName: "alert-triangle"; size: Theme.icon.xl; color: Theme.palette.textTertiary }
    }
}
