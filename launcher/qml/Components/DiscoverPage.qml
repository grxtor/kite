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

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.xl + Theme.space.xs
                Layout.rightMargin: Theme.space.xl + Theme.space.xs
                spacing: Theme.space.md

                SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 160
                    placeholderText: qsTr("Search modpacks on Modrinth")
                    onTextEdited: debounce.restart()
                    onTextChanged: if (text.length === 0) debounce.restart()
                }

                ComboBox {
                    id: sortBox
                    Layout.preferredWidth: 200
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
                    flat: false
                    iconName: "external-link"
                    tip: qsTr("CurseForge, FTB, ATLauncher and Technic, in the classic dialog")
                    onClicked: root.otherPlatformsRequested()
                }
            }

            SegmentedControl {
                Layout.leftMargin: Theme.space.xl + Theme.space.xs
                Layout.topMargin: -Theme.space.xs
                options: root.loaders
                current: root.model ? root.model.loader : ""
                onActivated: (value) => {
                    if (!root.model)
                        return
                    root.model.loader = value
                    root.runSearch()
                }
            }

            ListView {
                id: results
                Layout.fillWidth: true
                Layout.fillHeight: true
                leftMargin: Theme.space.xl + Theme.space.xs
                rightMargin: Theme.space.xl + Theme.space.xs
                bottomMargin: Theme.space.xl
                spacing: Theme.space.md
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.model
                ScrollBar.vertical: ScrollBar {}

                // QML views never ask for more on their own; ask as the end
                // comes into view.
                onAtYEndChanged: if (atYEnd && root.model && root.model.canFetchMore && !root.model.searching)
                                     root.model.fetchMore()

                delegate: ModpackRow {
                    width: results.width - results.leftMargin - results.rightMargin
                    onClicked: root.open({ projectId: projectId, title: title, author: author,
                                           description: description, logoUrl: logoUrl, downloads: downloads })
                }

                footer: Item {
                    width: results.width - results.leftMargin - results.rightMargin
                    height: root.model && root.model.searching ? 72 : Theme.space.md
                    BusyIndicator {
                        anchors.centerIn: parent
                        running: !!root.model && root.model.searching
                        visible: running
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
