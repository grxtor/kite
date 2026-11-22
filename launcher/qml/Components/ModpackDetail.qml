// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One modpack, opened from Discover, laid out like a project page: a header (over the gallery's featured picture
 * when there is one), then tabs (Description, Gallery, Versions, Contents) beside a side column that stays on every
 * tab and holds the page's one Install. `pack` is the opened row; `detail` is the model's lazily loaded body,
 * gallery, links and version list. The way back is the top bar's breadcrumb, Esc or Back (DiscoverPage).
 */
Item {
    id: root

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher; owned by C++ so installs survive leaving the page.
    property var installer: null
    // How `detail.body` reads: "markdown" (Modrinth) or "html" (CurseForge); the page shows `detail.bodyHtml`
    // either way.
    property string bodyFormat: "markdown"
    property var watcher: null
    // DiscoverPage's page frame, so the page lines up with its toolbar.
    property int contentX: Theme.space.xl + Theme.space.xs
    property int contentWidth: width - contentX * 2
    // A line of body text stays readable up to here; the column may be wider.
    readonly property int textMeasure: 720
    readonly property int sideWidth: 300
    readonly property int columnGap: Theme.space.xl
    readonly property bool twoColumns: root.contentWidth >= 820

    // "description" | "gallery" | "versions" | "contents"
    property string tab: "description"
    readonly property bool viewerOpen: viewer.opened

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property var gallery: detail && detail.gallery ? detail.gallery : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded
    readonly property var pickedVersion: side.versionIndex >= 0 && side.versionIndex < versions.length
                                         ? versions[side.versionIndex] : null
    // Only where the provider lists a version's projects without the pack file (Modrinth's dependencies).
    readonly property bool contentsListed: !!model && typeof model.loadContents === "function"
                                           && root.versions.some((v) => (v.contentCount || 0) > 0)

    readonly property var tabList: {
        const tabs = [{ id: "description", label: qsTr("Description") }]
        if (root.gallery.length > 0)
            tabs.push({ id: "gallery", label: qsTr("Gallery"), count: root.gallery.length })
        if (root.versions.length > 0)
            tabs.push({ id: "versions", label: qsTr("Versions"), count: root.versions.length })
        if (root.contentsListed)
            tabs.push({ id: "contents", label: qsTr("Contents"),
                        count: root.pickedVersion ? (root.pickedVersion.contentCount || 0) : -1 })
        return tabs
    }
    readonly property string shownTab: root.tabList.some((t) => t.id === root.tab) ? root.tab : "description"

    readonly property var loaderNames: ({ neoforge: "NeoForge", forge: "Forge", fabric: "Fabric",
                                          quilt: "Quilt", liteloader: "LiteLoader" })
    function loaderName(id) {
        return root.loaderNames[id] || Format.titleCase(id)
    }

    function distinct(field) {
        var seen = []
        for (var i = 0; i < root.versions.length; ++i) {
            var values = root.versions[i][field] || []
            for (var j = 0; j < values.length; ++j) {
                if (seen.indexOf(values[j]) < 0)
                    seen.push(values[j])
            }
        }
        return seen
    }
    readonly property var gameVersions: root.distinct("gameVersions")
    readonly property var loaderIds: root.distinct("loaders")

    // Many packs prefix versions with the pack's name ("All the Mods 10-8.3"); show "8.3" instead.
    function versionNumber(v) {
        var label = String(v.versionNumber || v.name || "")
        var title = root.pack && root.pack.title ? String(root.pack.title) : ""
        if (title.length > 0 && label.toLowerCase().startsWith(title.toLowerCase())) {
            var rest = label.slice(title.length).replace(/^[\s\-_:v]+/i, "")
            if (rest.length > 0)
                label = rest
        }
        return label
    }
    function versionLabel(v) {
        var parts = []
        if (v.gameVersions && v.gameVersions.length > 0)
            parts.push(v.gameVersions[0])
        if (v.loaders && v.loaders.length > 0)
            parts.push(v.loaders.map(l => root.loaderName(l)).join(", "))
        return root.versionNumber(v) + (parts.length > 0 ? " (" + parts.join(", ") + ")" : "")
    }

    function install() {
        if (!root.installer || !root.pickedVersion)
            return
        root.watcher = root.installer(root.pack.projectId, root.pickedVersion.id,
                                      side.instanceName.length > 0 ? side.instanceName : root.pack.title, "")
    }
    function showPicture(index) {
        viewer.show(root.gallery, index)
    }
    function loadContents() {
        if (root.shownTab === "contents" && root.pickedVersion && root.contentsListed)
            root.model.loadContents(root.pickedVersion.id)
    }
    onShownTabChanged: root.loadContents()
    onPickedVersionChanged: root.loadContents()

    onPackChanged: {
        watcher = null
        side.instanceName = pack.title || ""
        root.tab = "description"
        flick.contentY = 0
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.y + column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: root.contentX
            y: Theme.space.lg
            width: root.contentWidth
            spacing: Theme.space.xl

            ModpackDetailHeader {
                width: parent.width
                pack: root.pack
                // The row's small featured picture until the detail brings the gallery (featured first).
                cover: root.gallery.length > 0 ? (root.gallery[0].url || "") : (root.pack.galleryUrl || "")
            }

            NoticeBar {
                width: parent.width
                visible: !!root.watcher && root.watcher.failed
                text: root.watcher && root.watcher.error.length > 0
                      ? qsTr("The install failed: %1").arg(root.watcher.error)
                      : qsTr("The install failed.")
            }

            GridLayout {
                width: parent.width
                columns: root.twoColumns ? 2 : 1
                columnSpacing: root.columnGap
                rowSpacing: Theme.space.xl

                ModpackDetailSidebar {
                    id: side
                    Layout.row: 0
                    Layout.column: root.twoColumns ? 1 : 0
                    Layout.preferredWidth: root.twoColumns ? root.sideWidth : -1
                    Layout.fillWidth: !root.twoColumns
                    Layout.alignment: Qt.AlignTop
                    page: root
                }

                ColumnLayout {
                    id: main
                    Layout.row: root.twoColumns ? 0 : 1
                    Layout.column: 0
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.space.lg

                    TabStrip {
                        Layout.fillWidth: true
                        tabs: root.tabList
                        current: root.shownTab
                        onActivated: (id) => root.tab = id
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.shownTab === "description"
                        spacing: Theme.space.lg

                        GalleryStrip {
                            Layout.fillWidth: true
                            visible: root.gallery.length > 0
                            images: root.gallery
                            onOpened: (index) => root.showPicture(index)
                        }
                        BusyIndicator {
                            visible: !!root.detail && root.detail.loading
                            running: visible
                        }
                        // Not a Text: a failed remote image could crash it (see RemoteRichText). Held to a
                        // readable line; its pictures scale down to that width.
                        RemoteRichText {
                            Layout.fillWidth: true
                            Layout.maximumWidth: root.textMeasure
                            source: root.detail ? (root.detail.bodyHtml || "") : ""
                            format: "html"
                            article: true
                            onLinkActivated: (link) => ExternalLinks.open(root, link)
                        }
                        Copy {
                            Layout.fillWidth: true
                            visible: !!root.detail && !root.detail.loading && (root.detail.body || "").length === 0
                            text: qsTr("This pack has no description.")
                            tone: "tertiary"
                        }
                    }

                    ModpackGalleryGrid {
                        Layout.fillWidth: true
                        visible: root.shownTab === "gallery"
                        images: visible ? root.gallery : []
                        onOpened: (index) => root.showPicture(index)
                    }

                    ModpackVersionList {
                        Layout.fillWidth: true
                        visible: root.shownTab === "versions"
                        versions: visible ? root.versions : []
                        current: side.versionIndex
                        labelOf: root.versionNumber
                        onPicked: (index) => side.versionIndex = index
                    }

                    ModpackContentsList {
                        Layout.fillWidth: true
                        visible: root.shownTab === "contents"
                        items: visible && root.detail && root.pickedVersion
                               && root.detail.contentsVersionId === root.pickedVersion.id ? root.detail.contents : []
                        loading: !!root.detail && root.detail.contentsLoading
                        versionLabel: root.pickedVersion ? root.versionNumber(root.pickedVersion) : ""
                    }
                    Copy {
                        Layout.fillWidth: true
                        visible: root.shownTab === "contents" && !!root.detail && !root.detail.contentsLoading
                                 && !!root.pickedVersion && (root.pickedVersion.contentCount || 0) === 0
                        text: qsTr("This version does not list what it holds.")
                        tone: "tertiary"
                    }
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }

    GalleryViewer {
        id: viewer
        fallbackTitle: root.pack.title || ""
    }

    // Review snapshots: "packgallery" | "packversions" | "packcontents" open that tab, "packviewer" the first picture.
    function review(name) {
        if (name === "packviewer")
            root.showPicture(0)
        else if (name === "packversioncombo")
            side.openVersions()
        else
            root.tab = name.replace("pack", "")
    }
}
