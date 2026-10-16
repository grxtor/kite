// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Browse modpacks and install them as new instances, without leaving the
 * main window. Modrinth for now -- no account or key needed; a pack from
 * another platform (CurseForge, FTB, ATLauncher, Technic) can be imported
 * from its exported .zip via the New instance dialog's Import mode, one
 * click away; browsing those catalogues here is not built yet.
 *
 * `model` is the shell's ModrinthModpackModel: query/loader/sort in,
 * search()/fetchMore() to run, rows out, plus a lazily loaded `detail`.
 */
Item {
    id: root

    property var model: null
    property var installer: null
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
        if (root.model)
            root.model.loadDetail(pack.projectId)
    }

    // Set once a search has been asked for, so "nothing found" is never
    // shown for a search that has not happened.
    property bool searched: false

    function runSearch() {
        if (!root.model)
            return
        root.searched = true
        root.model.search()
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
            if (!root.model)
                return
            root.model.query = searchField.text
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
                    placeholderText: qsTr("Search modpacks on Modrinth")
                    onTextEdited: debounce.restart()
                    onTextChanged: if (text.length === 0) debounce.restart()
                }

                Row {
                    id: filters
                    x: toolbar.stacked ? 0 : importButton.x - toolbar.spacing - width
                    y: toolbar.stacked ? Theme.control.height + toolbar.rowSpacing : 0
                    spacing: toolbar.spacing

                    SegmentedControl {
                        id: loaderControl
                        options: root.loaders
                        current: root.model ? root.model.loader : ""
                        onActivated: (value) => {
                            if (!root.model)
                                return
                            root.model.loader = value
                            root.runSearch()
                        }
                    }

                    ComboBox {
                        id: sortBox
                        readonly property int preferredWidth: 180
                        width: preferredWidth
                        visible: count > 0
                        model: root.model && root.model.sortOptions ? root.model.sortOptions : []
                        textRole: "label"
                        valueRole: "id"
                        Accessible.name: qsTr("Sort modpacks by")
                        Component.onCompleted: if (root.model) currentIndex = Math.max(0, indexOfValue(root.model.sort))
                        onActivated: {
                            if (!root.model)
                                return
                            root.model.sort = currentValue
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
                onAtYEndChanged: if (atYEnd && root.model && root.model.canFetchMore && !root.model.searching)
                                     root.model.fetchMore()

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
                            model: root.model
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
                            model: root.model && root.model.searching
                                   ? (root.model.count === 0 ? flick.columns * 2 : flick.columns)
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
            detail: root.model ? root.model.detail : null
            model: root.model
            installer: root.installer
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
        visible: root.searched && !root.openedPack && !!root.model && !root.model.searching && root.model.count === 0
        title: root.model && root.model.error.length > 0 ? qsTr("Couldn't reach Modrinth")
                                                         : qsTr("No modpacks found")
        body: root.model && root.model.error.length > 0 ? root.model.error
                                                        : qsTr("Try fewer words, or another loader.")
        actionText: root.model && root.model.error.length > 0 ? qsTr("Try again") : ""
        actionIcon: "refresh"
        onActionTriggered: root.runSearch()

        // The same plain line icon Library's empty state uses: a search
        // glyph for "nothing matched", the warning glyph for a failed
        // request -- that is a fault, not an empty result.
        MeshIcon {
            iconName: root.model && root.model.error.length > 0 ? "alert-triangle" : "search"
            size: 40
            color: Theme.palette.textTertiary
        }
    }
}
