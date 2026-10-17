// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * One modpack, opened from Discover: what it is, which version to install,
 * and the install itself -- progress in place, then a way to the new
 * instance. `pack` is the row that was opened (title, author, logo,
 * galleryUrl, accentColor...); `detail` is the model's lazily loaded body,
 * gallery and version list. Provider-neutral: Modrinth's and CurseForge's
 * models give the same roles and the same `detail` shape; only the raw
 * body's format differs (`bodyFormat`), and the page reads `bodyHtml`, the
 * same text every model already rewrote into one plain HTML article.
 *
 * Install is the page's one primary button and sits in the header, where a
 * launcher's project page puts it; the Install card below only holds what
 * it installs (version, instance name) and how it is going.
 */
Item {
    id: root

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher, owned by C++ so
    // the install survives leaving this page.
    property var installer: null
    // "markdown" (Modrinth) or "html" (CurseForge): how `detail.body` reads.
    // The page itself shows `detail.bodyHtml`, which is HTML either way.
    property string bodyFormat: "markdown"
    // TaskWatcher of the install started from here, if any.
    property var watcher: null
    // DiscoverPage's page frame, so the detail lines up with its title.
    property int contentX: Theme.space.xl + Theme.space.xs
    property int contentWidth: width - contentX * 2
    // A reading page: narrower than the results grid on a wide window.
    readonly property int maxReadingWidth: 960
    // The description's own measure: lines of a comfortable length, and
    // the width its pictures scale down to.
    readonly property int textMeasure: 720

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property var gallery: detail && detail.gallery ? detail.gallery : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded

    // The best backdrop available right now: the detail's own gallery once
    // it has loaded (its featured image first, see the models' loadDetail()),
    // falling back to the card's cover so the header is never bare while
    // that request is still in flight.
    readonly property string backdropUrl: root.gallery.length > 0 ? root.gallery[0].url
                                         : (root.pack.galleryUrl || "")
    // The provider's ISO last-modified date as Format.lastPlayed() words it,
    // or ""
    // when there is none.
    readonly property string updatedText: {
        var ms = Date.parse(root.pack.updated || "")
        return ms > 0 ? Format.lastPlayed(ms) : ""
    }

    // Loader ids as the projects spell their own names.
    readonly property var loaderNames: ({ neoforge: "NeoForge", forge: "Forge", fabric: "Fabric",
                                          quilt: "Quilt", liteloader: "LiteLoader" })

    function versionLabel(v) {
        var parts = []
        if (v.gameVersions && v.gameVersions.length > 0)
            parts.push(v.gameVersions[0])
        if (v.loaders && v.loaders.length > 0)
            parts.push(v.loaders.map(l => root.loaderNames[l] || (l.charAt(0).toUpperCase() + l.slice(1))).join(", "))
        // Many packs number their versions with the pack's own name in front
        // ("All the Mods 10-8.3"); repeating it on every row of the list only
        // pushes the part that differs to the end. Show "8.3" instead.
        var label = String(v.versionNumber || v.name || "")
        var title = root.pack && root.pack.title ? String(root.pack.title) : ""
        if (title.length > 0 && label.toLowerCase().startsWith(title.toLowerCase())) {
            var rest = label.slice(title.length).replace(/^[\s\-_:v]+/i, "")
            if (rest.length > 0)
                label = rest
        }
        return label + (parts.length > 0 ? " (" + parts.join(", ") + ")" : "")
    }

    function install() {
        if (!root.installer || versionBox.currentIndex < 0)
            return
        var v = root.versions[versionBox.currentIndex]
        root.watcher = root.installer(root.pack.projectId, v.id,
                                      nameField.text.length > 0 ? nameField.text : root.pack.title, "")
    }

    onPackChanged: {
        watcher = null
        nameField.text = pack.title || ""
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: root.contentX
            width: Math.min(root.contentWidth, root.maxReadingWidth)
            spacing: Theme.space.xl

            // Pulled left by its own padding plus the chevron's side
            // bearing, so the chevron's ink -- not the button's hover
            // plate, nor the empty left third of the glyph's box -- starts
            // exactly on the page's left edge, like the hero below it.
            Button {
                // The chevron-left glyph's stroke starts 8 of its 24 units
                // in; rounded so the label stays on whole pixels.
                readonly property int chevronBearing: Math.round(Theme.icon.sm / 3)
                x: -(leftPadding + chevronBearing)
                flat: true
                text: qsTr("Back to results")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                onClicked: root.backRequested()
            }

            // Header: the gallery's own hero shot (or, without one, a
            // rendered scene picked by project id) behind a bottom scrim,
            // the logo, title/author, the headline stats and Install.
            Rectangle {
                id: hero
                width: parent.width
                height: Math.max(200, Math.min(320, Math.round(width * 0.32)))
                radius: Theme.radius.xl
                clip: true
                color: Theme.palette.surfaceSunken

                Image {
                    id: heroFallback
                    anchors.fill: parent
                    visible: backdrop.status !== Image.Ready
                    source: visible ? PixelArt.landscapeUrl(root.pack.projectId || "",
                                                            PixelArt.shapeForAspect(hero.width / Math.max(1, hero.height)))
                                    : ""
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }

                Image {
                    id: backdrop
                    anchors.fill: parent
                    source: root.backdropUrl
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow } }
                }

                // Bottom scrim so title/stats stay legible over any image.
                // Theme.media.scrim: the text on top is white in both
                // themes, so the fade is dark in both.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(Theme.media.scrim.r, Theme.media.scrim.g, Theme.media.scrim.b, 0) }
                        GradientStop { position: 0.55; color: Qt.rgba(Theme.media.scrim.r, Theme.media.scrim.g, Theme.media.scrim.b, 0.15) }
                        GradientStop { position: 1.0; color: Theme.media.scrim }
                    }
                }

                // The rounded mask: Rectangle's clip is square, so a ring in
                // the page colour covers the photo's four corners.
                Rectangle {
                    readonly property int ring: hero.radius + Theme.space.xxs
                    anchors.fill: parent
                    anchors.margins: -ring
                    radius: hero.radius + ring
                    color: "transparent"
                    border.width: ring
                    border.color: Theme.palette.canvas
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space.lg
                    spacing: Theme.space.lg

                    Item {
                        id: logoTile
                        readonly property int size: 88
                        readonly property int frame: Theme.space.xs
                        Layout.preferredWidth: size
                        Layout.preferredHeight: size
                        Layout.alignment: Qt.AlignBottom

                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius.lg
                            color: Theme.palette.surfaceSunken
                        }
                        Image {
                            id: logoImg
                            anchors.fill: parent
                            anchors.margins: logoTile.frame
                            source: root.pack.logoUrl || ""
                            sourceSize: Qt.size(width * 2, height * 2)
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            mipmap: true
                            asynchronous: true
                            visible: status === Image.Ready
                        }
                        MeshIcon {
                            anchors.centerIn: parent
                            visible: logoImg.status !== Image.Ready
                            iconName: "package"
                            size: Theme.icon.lg
                            color: Theme.palette.textTertiary
                        }
                        // The frame over the picture, which is also what
                        // rounds its corners (Rectangle's clip is square).
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius.lg
                            color: "transparent"
                            border.width: logoTile.frame
                            border.color: Theme.palette.surface
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignBottom
                        spacing: Theme.space.xs

                        Text {
                            Layout.fillWidth: true
                            text: root.pack.title || ""
                            elide: Text.ElideRight
                            color: Theme.media.text
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.display.pixelSize
                            font.weight: Font.Bold
                            font.letterSpacing: -0.4
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: !!root.pack.author
                            text: qsTr("by %1").arg(root.pack.author || "")
                            elide: Text.ElideRight
                            color: Theme.media.textSecondary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space.xxs
                            spacing: Theme.space.sm
                            Tag {
                                onMedia: true
                                // Not every provider counts them (ATLauncher).
                                visible: (root.pack.downloads || 0) > 0
                                iconName: "download"
                                text: qsTr("%1 downloads").arg(Format.compactNumber(root.pack.downloads))
                            }
                            Tag {
                                onMedia: true
                                visible: (root.pack.follows || 0) > 0
                                iconName: "users"
                                text: qsTr("%1 followers").arg(Format.compactNumber(root.pack.follows))
                            }
                            // Format's own wording, the same an instance's
                            // "last played" uses, behind a clock.
                            Tag {
                                onMedia: true
                                visible: root.updatedText.length > 0
                                iconName: "clock"
                                text: root.updatedText
                                Accessible.name: qsTr("Updated %1").arg(root.updatedText)
                            }
                        }
                    }

                    // The page's one primary action. Installs the version
                    // and name chosen in the Install card below -- the
                    // featured version and the pack's own name until changed.
                    Button {
                        Layout.alignment: Qt.AlignBottom
                        visible: !root.installed
                        enabled: !root.installing && versionBox.currentIndex >= 0
                        highlighted: true
                        implicitHeight: Theme.control.heightLg
                        text: root.installing ? qsTr("Installing…") : qsTr("Install")
                        icon.source: Icons.url("download")
                        onClicked: root.install()
                    }
                    Button {
                        Layout.alignment: Qt.AlignBottom
                        visible: root.installed
                        highlighted: true
                        implicitHeight: Theme.control.heightLg
                        text: qsTr("Show in library")
                        icon.source: Icons.url("library")
                        onClicked: root.showInstanceRequested(root.watcher.instanceId || "")
                    }
                }
            }

            // A failed install says so where the button that started it
            // is, not only in the Install card further down; Install can be
            // pressed again.
            NoticeBar {
                width: parent.width
                visible: !!root.watcher && root.watcher.failed
                text: root.watcher && root.watcher.error.length > 0
                      ? qsTr("The install failed: %1").arg(root.watcher.error)
                      : qsTr("The install failed.")
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.pack.description || ""
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                lineHeight: Theme.type.body.lineHeight
            }

            // The grid card only ever shows one chip; the full list lives
            // here (design-plan.md §5/§6), spaced like the hero's meta chips.
            Flow {
                width: parent.width
                spacing: Theme.space.sm
                visible: (root.pack.categories || []).length > 0
                Repeater {
                    model: root.pack.categories || []
                    delegate: Tag { text: modelData.length > 0 ? modelData.charAt(0).toUpperCase() + modelData.slice(1) : modelData }
                }
            }

            // Install: what the header's button installs, and its progress.
            SettingsGroup {
                id: installGroup
                width: parent.width
                title: qsTr("Install")

                SettingRow {
                    label: qsTr("Version")
                    description: root.detail && root.detail.loading ? qsTr("Loading versions…")
                               : root.versions.length === 0 ? qsTr("No versions found.") : ""
                    ComboBox {
                        id: versionBox
                        width: 320
                        enabled: root.versions.length > 0 && !root.installing
                        model: root.versions.map(v => root.versionLabel(v))
                        Accessible.name: qsTr("Version")
                        // Newest first from either provider, alphas included: start on
                        // the version the author features, if there is one.
                        onModelChanged: {
                            var featured = root.versions.findIndex(v => v.featured)
                            currentIndex = featured >= 0 ? featured : 0
                        }
                    }
                }

                SettingRow {
                    label: qsTr("Instance name")
                    TextField {
                        id: nameField
                        width: 320
                        enabled: !root.installing
                        selectByMouse: true
                        Accessible.name: qsTr("Instance name")
                    }
                }

                // Only once an install has started: progress, then the
                // outcome. Before that the header's button says it all.
                SettingRow {
                    visible: !!root.watcher
                    // The group draws its hairlines once, from which rows
                    // are visible then; this one appears later.
                    onVisibleChanged: installGroup.markRows()
                    label: root.installed ? qsTr("Installed")
                         : root.installing ? (root.watcher.status || qsTr("Installing…"))
                         : root.watcher && root.watcher.failed ? qsTr("Install failed")
                         : ""
                    description: root.watcher && root.watcher.failed ? root.watcher.error
                               : root.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""

                    LaunchProgressBar {
                        width: 180
                        visible: root.installing
                        progress: root.watcher ? root.watcher.progress : -1
                    }
                }
            }

            // Gallery strip -- only once the detail fetch actually has one.
            Column {
                width: parent.width
                spacing: Theme.space.sm
                visible: root.gallery.length > 0

                Text {
                    text: qsTr("Gallery")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                Flickable {
                    width: parent.width
                    height: galleryRow.height
                    contentWidth: galleryRow.width
                    contentHeight: height
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true

                    Row {
                        id: galleryRow
                        spacing: Theme.space.sm
                        Repeater {
                            model: root.gallery
                            delegate: Item {
                                id: shot
                                required property var modelData
                                readonly property int ring: Theme.radius.sm + Theme.space.xxs
                                width: 152
                                height: Math.round(width * 9 / 16)
                                clip: true

                                Rectangle {
                                    anchors.fill: parent
                                    color: Theme.palette.surfaceSunken
                                }
                                Image {
                                    anchors.fill: parent
                                    source: shot.modelData.url || ""
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                    mipmap: true
                                    asynchronous: true
                                }
                                // Rounded corners, the same page-colour ring
                                // the header uses.
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -shot.ring
                                    radius: Theme.radius.sm + shot.ring
                                    color: "transparent"
                                    border.width: shot.ring
                                    border.color: Theme.palette.canvas
                                }
                            }
                        }
                    }
                }
            }

            // About: the pack's own page, as RichBody rewrote it (see
            // RemoteRichText's `article`).
            Column {
                width: parent.width
                spacing: Theme.space.md
                visible: !!root.detail && ((root.detail.body || "").length > 0 || root.detail.loading)

                // Same heading as the Install card's title, on the same
                // left edge.
                Text {
                    text: qsTr("About")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Font.Bold
                }

                BusyIndicator {
                    visible: !!root.detail && root.detail.loading
                    running: visible
                }

                // Not a Text: the body links images from anywhere, and a
                // failed one could crash Text (see RemoteRichText).
                RemoteRichText {
                    width: Math.min(parent.width, root.textMeasure)
                    source: root.detail ? (root.detail.bodyHtml || "") : ""
                    format: "html"
                    article: true
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
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
}
