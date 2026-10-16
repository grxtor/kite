// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Browse modpacks and install them as new instances, without leaving the
 * main window, from Modrinth, CurseForge, FTB, ATLauncher or Technic -- a
 * provider switch lists whichever of them the shell hands this page a
 * model for. A pack exported from anywhere else can still be imported
 * from its .zip via the New instance dialog's Import mode.
 *
 * `model` is the shell's ModrinthModpackModel; the others are
 * FlameModpackModel, FtbModpackModel, AtlModpackModel and
 * TechnicModpackModel. All share the same properties, invokables and row
 * roles, so everything below works on `active`, whichever the switch
 * picked -- query/loader/sort in, search()/fetchMore() to run, rows out,
 * plus a lazily loaded `detail`. Each provider keeps its own results.
 * Filters a provider cannot answer (a loader on ATLauncher or Technic, a
 * sort on Technic) are left out rather than shown doing nothing.
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

    // Every provider the shell has a model for, in the switch's order.
    readonly property var providers: [
        { value: "modrinth", label: "Modrinth", model: root.model },
        { value: "curseforge", label: "CurseForge", model: root.curseForgeModel },
        { value: "ftb", label: "FTB", model: root.ftbModel },
        { value: "atlauncher", label: "ATLauncher", model: root.atlModel },
        { value: "technic", label: "Technic", model: root.technicModel }
    ].filter((p) => !!p.model)
    readonly property var current: root.providers.find((p) => p.value === root.provider) || root.providers[0] || null
    // The model the page is showing.
    readonly property var active: root.current ? root.current.model : null
    readonly property string providerName: root.current ? root.current.label : "Modrinth"
    readonly property bool keyMissing: !!root.active && root.active.keyMissing === true
    // ATLauncher and Technic say nothing about loaders; Technic has no sorts.
    readonly property bool loaderFilterAvailable: root.provider !== "atlauncher" && root.provider !== "technic"
    // Modrinth installs through the shell; the other models install
    // themselves (their TaskWatchers are parented to the model, so C++
    // owns them).
    readonly property var activeInstaller: root.provider === "modrinth"
        ? root.installer
        : function (projectId, versionId, name, group) { return root.active.install(projectId, versionId, name, group) }
    signal showInstanceRequested(string id)
    signal otherPlatformsRequested()

    property var openedPack: null

    // The same page frame as Library: content stops at
    // Theme.layout.maxContentWidth and centres on a wide window, under the
    // TopBar title that follows the same rule.
    readonly property int pagePadding: Theme.space.xl + Theme.space.xs
    readonly property int contentWidth: Math.max(0, Math.min(width - pagePadding * 2, Theme.layout.maxContentWidth))
    readonly property int contentX: Math.max(pagePadding, Math.round((width - contentWidth) / 2))

    readonly property var loaders: [
        { value: "", label: qsTr("Any") },
        { value: "fabric", label: "Fabric" },
        { value: "forge", label: "Forge" },
        { value: "neoforge", label: "NeoForge" },
        { value: "quilt", label: "Quilt" }
    ]

    // Opens the index-th result as if it were clicked.
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

    // The providers a search has been asked of, so "nothing found" is
    // never shown for a search that has not happened. Replaced, never
    // changed in place, so bindings see it.
    property var searchedProviders: []
    readonly property bool searched: root.searchedProviders.indexOf(root.provider) >= 0

    function runSearch() {
        if (!root.active)
            return
        if (!root.searched)
            root.searchedProviders = root.searchedProviders.concat([root.provider])
        root.active.search()
    }

    // Switches provider, carrying the search words and loader across, and
    // searches only if the other provider's results do not already answer
    // them.
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

    // First search when the page is first shown -- not at startup, so
    // opening the launcher never talks to Modrinth on its own. Gated on
    // StackLayout's own isCurrentItem (Qt 6.4+), not `visible`: `visible`
    // is still true at Component.onCompleted (StackLayout only flips the
    // non-current pages' visible off on its next polish, after every page's
    // own onCompleted has already run), so checking it here fired a search
    // on every startup regardless of which page was actually shown. Falls
    // back to `visible` if a future QtQuick.Layouts build ever drops the
    // attached property.
    function searchIfFirstShown() {
        var current = root.StackLayout && root.StackLayout.isCurrentItem !== undefined
                    ? root.StackLayout.isCurrentItem : root.visible
        if (current && !root.searched)
            runSearch()
    }
    StackLayout.onIsCurrentItemChanged: searchIfFirstShown()
    onModelChanged: searchIfFirstShown()
    Component.onCompleted: searchIfFirstShown()

    // Typing searches once the user pauses, not per keystroke.
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

        // Results
        ColumnLayout {
            spacing: Theme.space.lg

            // Search, loader, sort and import. One row while the search
            // field keeps a usable width; below that the loader/sort filters
            // move to a row of their own instead of squeezing the field to
            // "Search mo…". Import stays at the row's end either way -- the
            // same spot Library's "New instance" has.
            Item {
                id: toolbar
                Layout.leftMargin: root.contentX
                Layout.preferredWidth: root.contentWidth
                Layout.preferredHeight: stacked ? Theme.control.height * 2 + rowSpacing : Theme.control.height

                readonly property int spacing: Theme.space.md
                readonly property int rowSpacing: Theme.space.sm
                readonly property int searchMinWidth: 240
                readonly property bool stacked: width < searchMinWidth + filters.implicitWidth
                                                        + importButton.implicitWidth + spacing * 2

                SearchBox {
                    id: searchField
                    x: 0
                    y: 0
                    width: toolbar.stacked ? importButton.x - toolbar.spacing
                                           : filters.x - toolbar.spacing
                    placeholderText: qsTr("Search modpacks on %1").arg(root.providerName)
                    onTextEdited: debounce.restart()
                    onTextChanged: if (text.length === 0) debounce.restart()
                }

                Row {
                    id: filters
                    x: toolbar.stacked ? 0 : importButton.x - toolbar.spacing - width
                    y: toolbar.stacked ? Theme.control.height + toolbar.rowSpacing : 0
                    spacing: toolbar.spacing

                    // Where to browse; only once there is somewhere else.
                    SegmentedControl {
                        id: providerControl
                        visible: root.providers.length > 1
                        options: root.providers.map((p) => ({ value: p.value, label: p.label }))
                        current: root.current ? root.current.value : "modrinth"
                        onActivated: (value) => root.setProvider(value)
                    }

                    SegmentedControl {
                        id: loaderControl
                        visible: root.loaderFilterAvailable
                        options: root.loaders
                        current: root.active ? root.active.loader : ""
                        onActivated: (value) => {
                            if (!root.active)
                                return
                            root.active.loader = value
                            root.runSearch()
                        }
                    }

                    ComboBox {
                        id: sortBox
                        readonly property int preferredWidth: 180
                        width: preferredWidth
                        // One sort is no choice (Technic).
                        visible: count > 1
                        model: root.active && root.active.sortOptions ? root.active.sortOptions : []
                        textRole: "label"
                        valueRole: "id"
                        Accessible.name: qsTr("Sort modpacks by")
                        // Each provider has its own sorts and its own pick;
                        // re-read once the box has the new provider's entries
                        // (indexOfValue() needs them in place).
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
                }

                Button {
                    id: importButton
                    x: toolbar.width - width
                    y: 0
                    text: qsTr("Import")
                    icon.source: Icons.url("archive")
                    ToolTip.visible: hovered
                    ToolTip.delay: 400
                    ToolTip.text: qsTr("Import a pack exported from CurseForge, FTB, ATLauncher or Technic")
                    onClicked: root.otherPlatformsRequested()
                }
            }

            // Results, as a responsive card grid -- 2 to 4 columns
            // depending on width, the same fill-the-row math LibraryPage
            // uses for the instance grid.
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

                readonly property int gutter: Theme.space.lg
                readonly property int minCardWidth: 260
                readonly property int maxCardWidth: 340
                readonly property int gridWidth: root.contentWidth
                readonly property int columns: Math.max(2, Math.min(4, columnsFor(gridWidth)))
                readonly property int cardWidth: Math.floor((gridWidth - gutter * (columns - 1)) / columns)
                // The last column takes the pixels flooring left over, so
                // every row ends exactly on the toolbar's right edge.
                readonly property int lastCardWidth: gridWidth - (columns - 1) * (cardWidth + gutter)

                // Width of the card at grid slot `slot` (results first, then
                // any loading placeholders after them).
                function cardWidthAt(slot) {
                    return slot % columns === columns - 1 ? lastCardWidth : cardWidth
                }

                function columnsFor(w) {
                    var columns = Math.max(1, Math.floor((w + gutter) / (minCardWidth + gutter)))
                    while ((w - gutter * (columns - 1)) / columns > maxCardWidth)
                        columns++
                    return columns
                }

                Item {
                    id: content
                    x: root.contentX
                    // Room above the first row for a card's focus ring,
                    // which the flickable's clip would cut.
                    y: Theme.space.sm
                    width: flick.gridWidth
                    height: grid.height

                    Grid {
                        id: grid
                        width: parent.width
                        columns: flick.columns
                        columnSpacing: flick.gutter
                        rowSpacing: flick.gutter

                        Repeater {
                            id: resultsRepeater
                            model: root.active
                            delegate: ModpackCard {
                                width: flick.cardWidthAt(index)
                                // From the common width (see ModpackCard's
                                // coverHeight), not this card's own.
                                coverHeight: Math.round(flick.cardWidth * coverRatio)
                                projectId: model.projectId
                                title: model.title
                                author: model.author
                                description: model.description
                                logoUrl: model.logoUrl
                                downloads: model.downloads
                                updated: model.updated
                                // One chip per grid card (design-plan.md
                                // §5/§6); the full list is on the detail
                                // view instead (see ModpackDetail.qml).
                                categories: (model.categories || []).slice(0, 1)
                                galleryUrl: model.galleryUrl
                                accentColor: model.accentColor
                                onClicked: root.open({ projectId: projectId, title: title, author: author,
                                                       description: description, logoUrl: logoUrl,
                                                       downloads: downloads, updated: updated,
                                                       // The card's own `categories` is already
                                                       // capped to one chip above -- the detail
                                                       // view wants the model's full list.
                                                       categories: model.categories, galleryUrl: galleryUrl,
                                                       accentColor: accentColor })
                            }
                        }

                        // A couple of rows of shimmering placeholders while
                        // the first page is still loading, or one trailing
                        // row while a further page is being fetched --
                        // never a bare spinner.
                        Repeater {
                            model: root.active && root.active.searching
                                   ? (root.active.count === 0 ? flick.columns * 2 : flick.columns)
                                   : 0
                            delegate: ModpackCard {
                                required property int index
                                width: flick.cardWidthAt(resultsRepeater.count + index)
                                // From the common width (see ModpackCard's
                                // coverHeight), not this card's own.
                                coverHeight: Math.round(flick.cardWidth * coverRatio)
                                skeleton: true
                            }
                        }
                    }
                }
            }
        }

        // Detail
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

    // The results' last visible row fades out at the play bar instead of
    // being cut (the detail view has its own).
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !root.openedPack && flick.contentY + flick.height < flick.contentHeight - 1
        flickable: flick
    }

    EmptyState {
        anchors.centerIn: parent
        visible: root.searched && !root.keyMissing && !root.openedPack && !!root.active
                 && !root.active.searching && root.active.count === 0
        title: root.active && root.active.error.length > 0 ? qsTr("Couldn't reach %1").arg(root.providerName)
                                                           : qsTr("No modpacks found")
        body: root.active && root.active.error.length > 0 ? root.active.error
                                                          : qsTr("Try fewer words, or another loader.")
        actionText: root.active && root.active.error.length > 0 ? qsTr("Try again") : ""
        actionIcon: "refresh"
        onActionTriggered: root.runSearch()

        // The same plain line icon Library's empty state uses: a search
        // glyph for "nothing matched", the warning glyph for a failed
        // request -- that is a fault, not an empty result.
        MeshIcon {
            iconName: root.active && root.active.error.length > 0 ? "alert-triangle" : "search"
            size: 40
            color: Theme.palette.textTertiary
        }
    }

    // A build without a CurseForge API key cannot ask CurseForge anything;
    // say so rather than show a failed search. (Only CurseForge needs a
    // key.)
    EmptyState {
        anchors.centerIn: parent
        width: parent.width
        visible: root.keyMissing && !root.openedPack
        title: qsTr("CurseForge isn't available")
        body: qsTr("This build has no CurseForge API key, so its modpacks can't be browsed here. Modrinth works as usual.")
        actionText: qsTr("Browse Modrinth")
        actionIcon: "compass"
        onActionTriggered: root.setProvider("modrinth")
        MeshIcon { iconName: "alert-triangle"; size: 40; color: Theme.palette.textTertiary }
    }
}
