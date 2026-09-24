// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Where this instance's modpack came from, and what other versions of it
 * exist -- the widget-free replacement for ManagedPackPage, scoped to
 * instances with a catalogue id (see ManagedPackController's class
 * comment for what is deliberately not reproduced here).
 */
Item {
    id: root

    // InstanceDetails.managedPack (ManagedPackController), or null.
    property var controller: null
    // The TaskWatcher of an update in progress, if any.
    property var watcher: null
    readonly property bool updating: !!root.watcher && root.watcher.running

    // Emitted once an update has landed on disk: this instance has just
    // been replaced, so the page showing it -- including this tab's own
    // controller -- is no longer trustworthy to keep looking at. See
    // ManagedPackController::updateToVersion()'s own comment.
    signal updated()

    // Fires once at creation (the property's initial binding still counts
    // as a change from its declared `null` default) and again every time
    // this tab's instance page opens a different instance - see
    // VersionTab.qml's minecraftVersions binding for the same "read/bind
    // triggers the fetch" idiom, here made explicit since fetchVersions()
    // is a method, not a property getter.
    onControllerChanged: {
        root.watcher = null
        if (root.controller)
            root.controller.fetchVersions()
    }

    // The lowercase URI scheme of "s" (e.g. "https" for "https://x"), or
    // null if it has none.
    function schemeOf(s) {
        var m = /^([a-zA-Z][a-zA-Z0-9+.-]*):/.exec(String(s))
        return m ? m[1].toLowerCase() : null
    }

    /*
     * Mirrors ManagedPackPage.cpp's anchorClicked guard exactly: a
     * changelog is remote, untrusted text, and handing an arbitrary
     * scheme to the desktop is how a "file:" or worse ends up being
     * opened, so only http(s) links are ever passed on. A schemeless
     * link is CurseForge's outbound redirect, which in changelog HTML
     * arrives as the relative "linkout?remoteUrl=<percent-encoded>" --
     * the real destination is in the query, decoded and scheme-checked
     * the same way before it is allowed through.
     */
    function openChangelogLink(link) {
        var url = String(link)
        var scheme = root.schemeOf(url)
        if (scheme !== null) {
            if (scheme === "http" || scheme === "https")
                Qt.openUrlExternally(url)
            else
                console.warn("ManagedPackTab: refusing to open changelog link with scheme:", scheme)
            return
        }

        var queryIndex = url.indexOf("?")
        var remote = ""
        if (queryIndex >= 0) {
            var parts = url.substring(queryIndex + 1).split("&")
            for (var i = 0; i < parts.length; ++i) {
                var eq = parts[i].indexOf("=")
                var key = eq >= 0 ? parts[i].substring(0, eq) : parts[i]
                if (key === "remoteUrl") {
                    var value = eq >= 0 ? parts[i].substring(eq + 1) : ""
                    try {
                        remote = decodeURIComponent(decodeURIComponent(value))
                    } catch (e) {
                        remote = ""
                    }
                    break
                }
            }
        }

        var remoteScheme = root.schemeOf(remote)
        if (remoteScheme === "http" || remoteScheme === "https")
            Qt.openUrlExternally(remote)
        else if (remote.length > 0)
            console.warn("ManagedPackTab: refusing to open changelog redirect with scheme:", remoteScheme)
    }

    Connections {
        target: root.watcher
        ignoreUnknownSignals: true
        function onFinished(ok) {
            if (ok)
                root.updated()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: !!root.controller

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            Rectangle {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                radius: Theme.radius.md
                color: Theme.palette.surfaceSunken
                MeshIcon { anchors.centerIn: parent; iconName: "package"; size: Theme.icon.sm; color: Theme.palette.textTertiary }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: root.controller ? root.controller.packName : ""
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }
                RowLayout {
                    spacing: Theme.space.sm
                    Tag { text: root.controller ? root.controller.providerLabel : "" }
                    Text {
                        text: root.controller && root.controller.installedVersionName.length > 0
                              ? qsTr("Installed: %1").arg(root.controller.installedVersionName)
                              : qsTr("Installed version unknown")
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }

            Button {
                text: qsTr("Website")
                icon.source: Icons.url("external-link")
                visible: root.controller && root.controller.packUrl.length > 0
                onClicked: Qt.openUrlExternally(root.controller.packUrl)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.updating || (!!root.watcher && root.watcher.failed)
            spacing: Theme.space.md

            Text {
                Layout.fillWidth: true
                text: root.watcher
                      ? (root.watcher.failed ? (root.watcher.error || qsTr("Update failed."))
                                             : (root.watcher.status || qsTr("Updating…")))
                      : ""
                color: root.watcher && root.watcher.failed ? Theme.palette.danger : Theme.palette.textSecondary
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            LaunchProgressBar {
                Layout.preferredWidth: 160
                visible: root.updating
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.controller && !root.controller.hasPackId
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("MeshMC does not have a catalogue id on record for this pack, so it cannot list other versions here. Install the pack again from Discover if you need a specific version.")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.controller && root.controller.hasPackId && root.controller.error.length > 0
            spacing: Theme.space.sm
            // What went wrong in plain words first, then the catalogue's own
            // message (a raw job string, useful for a bug report) underneath.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Could not load the versions of this pack. Check your connection, then reload.")
                    color: Theme.palette.danger
                    elide: Text.ElideRight
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                Text {
                    Layout.fillWidth: true
                    text: root.controller ? root.controller.error : ""
                    color: Theme.palette.textTertiary
                    elide: Text.ElideMiddle
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
            Button {
                text: qsTr("Reload")
                icon.source: Icons.url("refresh")
                onClicked: root.controller.reload()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.controller && root.controller.hasPackId
            spacing: Theme.space.md

            ListView {
                id: list
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                clip: true
                spacing: Theme.space.xs
                boundsBehavior: Flickable.StopAtBounds
                model: root.controller ? root.controller.versions : []
                // A plain initial value, not a lasting binding: tapping a
                // row below assigns this directly, which is what a plain
                // QML property assignment does to whatever binding came
                // before it - exactly what is wanted here, since the
                // newest version (index 0) is the right thing to default
                // to only until the user actually picks one themselves.
                currentIndex: (root.controller && root.controller.versions.length > 0) ? 0 : -1
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: verRow
                    required property int index
                    required property var modelData

                    width: list.width - Theme.space.sm
                    height: Theme.control.heightLg
                    radius: Theme.radius.md
                    color: list.currentIndex === index ? Theme.palette.surfaceRaised : "transparent"
                    border.width: list.currentIndex === index ? 1 : 0
                    border.color: Theme.palette.border

                    TapHandler { onTapped: list.currentIndex = verRow.index }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space.sm
                        anchors.rightMargin: Theme.space.sm
                        Text {
                            Layout.fillWidth: true
                            text: verRow.modelData.label
                            elide: Text.ElideRight
                            color: verRow.modelData.installable ? Theme.palette.textPrimary : Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                        Tag { visible: verRow.modelData.current; text: qsTr("Current") }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space.sm

                Rectangle {
                    id: changelogHost
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Theme.radius.lg
                    color: Theme.palette.surfaceSunken
                    border.width: 1
                    border.color: Theme.palette.border

                    readonly property var selected: {
                        const rows = root.controller ? root.controller.versions : []
                        return list.currentIndex >= 0 && list.currentIndex < rows.length ? rows[list.currentIndex] : null
                    }

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: Theme.space.md
                        contentWidth: width
                        contentHeight: changelog.implicitHeight
                        clip: true
                        ScrollBar.vertical: ScrollBar {}

                        Text {
                            id: changelog
                            width: parent.width
                            text: changelogHost.selected && changelogHost.selected.changelog.length > 0
                                  ? changelogHost.selected.changelog
                                  : qsTr("No changelog available for this version.")
                            textFormat: Text.MarkdownText
                            wrapMode: Text.Wrap
                            color: Theme.palette.textSecondary
                            linkColor: Theme.palette.accent
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            onLinkActivated: (link) => root.openChangelogLink(link)
                        }
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    enabled: !root.updating && list.currentIndex >= 0 &&
                             root.controller && root.controller.versions.length > 0 &&
                             root.controller.versions[list.currentIndex] &&
                             root.controller.versions[list.currentIndex].installable &&
                             !root.controller.versions[list.currentIndex].current
                    text: qsTr("Update to this version")
                    icon.source: Icons.url("download")
                    onClicked: root.watcher = root.controller.updateToVersion(list.currentIndex)
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        visible: root.controller && root.controller.loading
        running: visible
    }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: !root.controller
        title: qsTr("Not a managed pack")
        body: qsTr("This instance was not installed from a catalogue MeshMC recognises.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }
}
