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
 * gallery and version list.
 */
Item {
    id: root

    property var pack: ({})
    property var detail: null
    property var model: null
    // (projectId, versionId, name, group) -> TaskWatcher. The shell's, so
    // the install is owned by C++ and survives leaving this page.
    property var installer: null
    // TaskWatcher of the install started from here, if any.
    property var watcher: null

    signal backRequested()
    signal showInstanceRequested(string id)

    readonly property var versions: detail && detail.versions ? detail.versions : []
    readonly property var gallery: detail && detail.gallery ? detail.gallery : []
    readonly property bool installing: !!watcher && watcher.running
    readonly property bool installed: !!watcher && watcher.succeeded

    readonly property color tint: root.pack.accentColor ? root.pack.accentColor : Theme.palette.accent
    // The best backdrop available right now: the detail's own gallery once
    // it has loaded (its featured image first, see ModrinthModpackModel),
    // falling back to the card's cover so the header is never bare while
    // that request is still in flight.
    readonly property string backdropUrl: root.gallery.length > 0 ? root.gallery[0].url
                                         : (root.pack.galleryUrl || "")

    function versionLabel(v) {
        var parts = []
        if (v.gameVersions && v.gameVersions.length > 0)
            parts.push(v.gameVersions[0])
        if (v.loaders && v.loaders.length > 0)
            parts.push(v.loaders.map(l => l.charAt(0).toUpperCase() + l.slice(1)).join(", "))
        return (v.versionNumber || v.name) + (parts.length > 0 ? "  ·  " + parts.join(" · ") : "")
    }

    // Modrinth's ISO date -> "3 h ago" / "5 d ago". Duplicated from
    // ModpackCard rather than shared: both are a handful of lines, and
    // Format.qml (the natural shared home) belongs to a different area.
    function timeAgo(iso) {
        if (!iso)
            return ""
        var ms = Date.parse(iso)
        if (isNaN(ms))
            return ""
        var minutes = Math.max(0, Math.floor((Date.now() - ms) / 60000))
        if (minutes < 60)
            return qsTr("just now")
        var hours = Math.floor(minutes / 60)
        if (hours < 24)
            return qsTr("%1 h ago").arg(hours)
        var days = Math.floor(hours / 24)
        if (days < 30)
            return qsTr("%1 d ago").arg(days)
        var months = Math.floor(days / 30)
        if (months < 12)
            return qsTr("%1 mo ago").arg(months)
        return qsTr("%1 y ago").arg(Math.floor(months / 12))
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
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + Theme.space.xxl
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column
            x: Theme.space.xl + Theme.space.xs
            width: Math.min(parent.width - x * 2, 960)
            spacing: Theme.space.xl

            Button {
                flat: true
                text: qsTr("Back to results")
                icon.source: Icons.url("chevron-left")
                leftPadding: Theme.space.sm
                onClicked: root.backRequested()
            }

            // Cinematic header: the gallery's own hero shot (or a fallback
            // gradient cut from the project's accent colour) behind a
            // bottom scrim, the logo, title/author and the headline stats.
            Rectangle {
                id: hero
                width: parent.width
                height: Math.max(200, Math.min(320, Math.round(width * 0.32)))
                radius: Theme.radius.xl
                clip: true
                color: Theme.palette.surfaceSunken

                Rectangle {
                    id: heroFallback
                    anchors.fill: parent
                    visible: backdrop.status !== Image.Ready
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Format.shade(root.tint, Theme.dark ? 0.30 : 0.88, 0.9) }
                        GradientStop { position: 1.0; color: Format.shade(root.tint, Theme.dark ? 0.12 : 0.70, 0.85) }
                    }
                    Image {
                        anchors.centerIn: parent
                        width: parent.height * 1.3
                        height: width
                        source: root.pack.logoUrl || ""
                        sourceSize: Qt.size(120, 120)
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        opacity: 0.18
                        visible: status === Image.Ready
                    }
                }

                Image {
                    id: backdrop
                    anchors.fill: parent
                    source: root.backdropUrl
                    sourceSize: Qt.size(1024, 480)
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    opacity: status === Image.Ready ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow } }
                }

                // Bottom scrim so title/stats stay legible over any image.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
                        GradientStop { position: 0.55; color: Qt.rgba(0, 0, 0, 0.15) }
                        GradientStop { position: 1.0; color: Theme.palette.scrim }
                    }
                }

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.space.lg
                    spacing: Theme.space.lg

                    // A slightly larger, translucent copy sitting behind
                    // the logo as a soft drop shadow -- layered rectangles
                    // standing in for a shader-based blur this Qt floor
                    // does not have.
                    Item {
                        Layout.preferredWidth: 88
                        Layout.preferredHeight: 88

                        Rectangle {
                            x: 3; y: 4
                            width: parent.width; height: parent.height
                            radius: Theme.radius.xl
                            color: Qt.rgba(0, 0, 0, 0.35)
                        }
                        Rectangle {
                            width: 88; height: 88
                            radius: Theme.radius.xl
                            color: Theme.palette.surface
                            border.width: 3
                            border.color: Theme.palette.surface

                            Image {
                                id: logoImg
                                anchors.fill: parent
                                anchors.margins: 3
                                source: root.pack.logoUrl || ""
                                sourceSize: Qt.size(176, 176)
                                fillMode: Image.PreserveAspectCrop
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
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.xs

                        Text {
                            Layout.fillWidth: true
                            text: root.pack.title || ""
                            elide: Text.ElideRight
                            color: Qt.rgba(1, 1, 1, 1)
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
                            color: Qt.rgba(1, 1, 1, 0.78)
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                        }
                        Row {
                            Layout.topMargin: Theme.space.xxs
                            spacing: Theme.space.sm
                            Tag {
                                onMedia: true
                                iconName: "download"
                                text: qsTr("%1 downloads").arg(Format.compactNumber(root.pack.downloads))
                            }
                            Tag {
                                onMedia: true
                                visible: (root.pack.follows || 0) > 0
                                iconName: "users"
                                text: qsTr("%1 followers").arg(Format.compactNumber(root.pack.follows))
                            }
                            Tag {
                                onMedia: true
                                visible: (root.pack.updated || "").length > 0
                                iconName: "clock"
                                text: qsTr("Updated %1").arg(root.timeAgo(root.pack.updated))
                            }
                        }
                    }
                }
            }

            // Gallery strip -- only once the detail fetch actually has one.
            Flickable {
                width: parent.width
                height: root.gallery.length > 0 ? 96 : 0
                visible: root.gallery.length > 0
                contentWidth: galleryRow.width
                contentHeight: height
                boundsBehavior: Flickable.StopAtBounds
                clip: true

                Row {
                    id: galleryRow
                    spacing: Theme.space.sm
                    Repeater {
                        model: root.gallery
                        delegate: Rectangle {
                            required property var modelData
                            width: 152
                            height: 86
                            radius: Theme.radius.md
                            color: Theme.palette.surfaceSunken
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: modelData.url || ""
                                sourceSize: Qt.size(304, 172)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }
                    }
                }
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.pack.description || ""
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize + 1
                lineHeight: 1.35
            }

            // The grid card only ever shows one chip; the full list lives
            // here (design-plan.md §5/§6).
            Flow {
                width: parent.width
                spacing: Theme.space.xs
                visible: (root.pack.categories || []).length > 0
                Repeater {
                    model: root.pack.categories || []
                    delegate: Tag { text: modelData.length > 0 ? modelData.charAt(0).toUpperCase() + modelData.slice(1) : modelData }
                }
            }

            // Install
            SettingsGroup {
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
                        // Newest first from Modrinth, alphas included: start on
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
                    }
                }

                SettingRow {
                    label: root.installed ? qsTr("Installed")
                         : root.installing ? (root.watcher.status || qsTr("Installing…"))
                         : root.watcher && root.watcher.failed ? qsTr("Install failed")
                         : qsTr("Ready to install")
                    description: root.watcher && root.watcher.failed ? root.watcher.error
                               : root.installed ? qsTr("“%1” is in your library.").arg(nameField.text) : ""

                    Row {
                        spacing: Theme.space.md

                        LaunchProgressBar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 180
                            visible: root.installing
                            progress: root.watcher ? root.watcher.progress : -1
                        }

                        Button {
                            visible: !root.installed
                            enabled: !root.installing && versionBox.currentIndex >= 0
                            highlighted: true
                            implicitHeight: Theme.control.heightLg
                            text: root.installing ? qsTr("Installing…") : qsTr("Install")
                            icon.source: Icons.url("download")
                            onClicked: root.install()
                        }

                        Button {
                            visible: root.installed
                            highlighted: true
                            implicitHeight: Theme.control.heightLg
                            text: qsTr("Show in library")
                            icon.source: Icons.url("library")
                            onClicked: root.showInstanceRequested(root.watcher.instanceId || "")
                        }
                    }
                }
            }

            // About
            Column {
                width: parent.width
                spacing: Theme.space.md
                visible: !!root.detail && ((root.detail.body || "").length > 0 || root.detail.loading)

                Text {
                    leftPadding: Theme.space.xs
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

                Text {
                    width: parent.width
                    text: root.detail ? (root.detail.body || "") : ""
                    textFormat: Text.MarkdownText
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                    linkColor: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }
    }
}
