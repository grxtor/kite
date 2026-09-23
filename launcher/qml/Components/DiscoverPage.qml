// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Browse modpacks and install them as new instances, without leaving the
 * main window. Modrinth for now -- no account or key needed; the other
 * platforms are still in the classic new-instance dialog, one click away.
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
    // opening the launcher never talks to Modrinth on its own.
    function searchIfFirstShown() {
        if (visible && !root.searched)
            runSearch()
    }
    onVisibleChanged: searchIfFirstShown()
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

            // Search, loader and sort in one toolbar; wraps onto a second
            // line once a narrow window can no longer fit all four
            // controls -- the search box gives up its width first, down to
            // a floor, before anything is pushed to the next line.
            Flow {
                id: toolbar
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.xl + Theme.space.xs
                Layout.rightMargin: Theme.space.xl + Theme.space.xs
                spacing: Theme.space.md

                readonly property int reserved: loaderControl.implicitWidth + sortBox.implicitWidth
                                                + otherPlatformsButton.implicitWidth + spacing * 3

                SearchBox {
                    id: searchField
                    width: Math.max(220, toolbar.width - toolbar.reserved)
                    placeholderText: qsTr("Search modpacks on Modrinth")
                    onTextEdited: debounce.restart()
                    onTextChanged: if (text.length === 0) debounce.restart()
                }

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
                    width: 200
                    visible: count > 0
                    model: root.model && root.model.sortOptions ? root.model.sortOptions : []
                    textRole: "label"
                    valueRole: "id"
                    Component.onCompleted: if (root.model) currentIndex = Math.max(0, indexOfValue(root.model.sort))
                    onActivated: {
                        if (!root.model)
                            return
                        root.model.sort = currentValue
                        root.runSearch()
                    }
                }

                IconButton {
                    id: otherPlatformsButton
                    flat: false
                    iconName: "external-link"
                    tip: qsTr("CurseForge, FTB, ATLauncher and Technic, in the classic dialog")
                    onClicked: root.otherPlatformsRequested()
                }
            }

            // Results, as a responsive card grid -- 2 to 4 columns
            // depending on width, same breakpoint math LibraryPage uses
            // for the instance grid.
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

                readonly property int pagePadding: Theme.space.xl + Theme.space.xs
                readonly property int gutter: Theme.space.lg
                readonly property int minCardWidth: 260
                readonly property int maxCardWidth: 340
                readonly property int gridWidth: Math.max(0, width - pagePadding * 2)
                readonly property int columns: Math.max(2, Math.min(4, columnsFor(gridWidth)))
                readonly property real cardWidth: Math.floor((gridWidth - gutter * (columns - 1)) / columns)

                function columnsFor(w) {
                    var columns = Math.max(1, Math.floor((w + gutter) / (minCardWidth + gutter)))
                    while ((w - gutter * (columns - 1)) / columns > maxCardWidth)
                        columns++
                    return columns
                }

                Item {
                    id: content
                    x: flick.pagePadding
                    y: Theme.space.xs
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
                                width: flick.cardWidth
                                projectId: model.projectId
                                title: model.title
                                author: model.author
                                description: model.description
                                logoUrl: model.logoUrl
                                downloads: model.downloads
                                updated: model.updated
                                categories: model.categories
                                galleryUrl: model.galleryUrl
                                accentColor: model.accentColor
                                onClicked: root.open({ projectId: projectId, title: title, author: author,
                                                       description: description, logoUrl: logoUrl,
                                                       downloads: downloads, updated: updated,
                                                       categories: categories, galleryUrl: galleryUrl,
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
                                width: flick.cardWidth
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
            onBackRequested: root.openedPack = null
            onShowInstanceRequested: (id) => root.showInstanceRequested(id)
        }
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

        MeshIcon {
            iconName: root.model && root.model.error.length > 0 ? "alert-triangle" : "search"
            size: 40
            color: Theme.palette.textTertiary
        }
    }
}
