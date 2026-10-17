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
 * provider drop-down lists whichever of them the shell hands this page a
 * model for. One toolbar row and a table of results (ModpackCard rows), as
 * on Modrinth and in the CurseForge app. A pack exported from anywhere else can still be imported
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

    // The toolbar's sizes: the boxes keep a readable width and the search
    // field takes the rest; on a narrow page the boxes step down first, so
    // the field keeps room for its placeholder.
    readonly property bool narrowToolbar: root.contentWidth < 820
    readonly property int providerWidth: root.narrowToolbar ? 124 : 150
    readonly property int sortWidth: root.narrowToolbar ? 164 : 190
    readonly property int searchMinWidth: 160
    readonly property int filterPanelWidth: 240
    // Placeholder rows while the first page loads.
    readonly property int skeletonRows: 6

    readonly property var loaders: [
        { value: "", label: qsTr("Any loader") },
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

            // Where, what, in which order, narrowed how -- one row at every
            // width, the same order as Add content's (ContentBrowserView):
            // provider, search, sort, Filters; Import at the row's end, the
            // spot Library's "New instance" has. The search field gives way
            // first.
            RowLayout {
                id: toolbar
                Layout.leftMargin: root.contentX
                Layout.preferredWidth: root.contentWidth
                Layout.maximumWidth: root.contentWidth
                spacing: Theme.space.md

                // Where to browse; only once there is somewhere else.
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

                // The loader, behind Filters as on Add content -- one
                // pattern for one kind of choice, not a second pill bar.
                Button {
                    id: filtersButton
                    visible: root.loaderFilterAvailable
                    readonly property bool narrowed: !!root.active && root.active.loader.length > 0
                    text: narrowed ? qsTr("Filters (1)") : qsTr("Filters")
                    icon.source: Icons.url("filter")
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

                            Text {
                                Layout.fillWidth: true
                                Layout.bottomMargin: Theme.space.xs
                                text: qsTr("Loader")
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.bodyStrong.pixelSize
                                font.weight: Theme.type.bodyStrong.weight
                            }
                            Repeater {
                                model: root.loaders
                                delegate: RadioButton {
                                    id: loaderRadio
                                    required property var modelData
                                    readonly property bool current: !!root.active && root.active.loader === modelData.value
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    // Checked follows the provider's own
                                    // loader (each provider keeps its own); a
                                    // click only asks, and puts the binding
                                    // it breaks back.
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

            // Results, as one table of rows (ModpackCard), like Modrinth's
            // and the CurseForge app's lists.
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

                Rectangle {
                    id: content
                    x: root.contentX
                    width: root.contentWidth
                    height: rows.height + border.width * 2 + Theme.space.xs * 2
                    visible: resultsRepeater.count + flick.placeholderRows > 0
                    radius: Theme.radius.lg
                    color: Theme.palette.surface
                    border.width: 1
                    border.color: Theme.palette.border

                    Column {
                        id: rows
                        x: content.border.width
                        y: content.border.width + Theme.space.xs
                        width: parent.width - content.border.width * 2

                        Repeater {
                            id: resultsRepeater
                            model: root.active
                            delegate: ModpackCard {
                                width: rows.width
                                lastRow: index === resultsRepeater.count - 1 && flick.placeholderRows === 0
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

                        // Shimmering rows while the first page is still
                        // loading, or one trailing row while a further page
                        // is being fetched -- never a bare spinner.
                        Repeater {
                            model: flick.placeholderRows
                            delegate: ModpackCard {
                                required property int index
                                width: rows.width
                                lastRow: index === flick.placeholderRows - 1
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
