// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Where this instance's modpack came from, and what other versions of it
 * exist -- the widget-free replacement for ManagedPackPage, scoped to
 * instances with a catalogue id (see ManagedPackController's class
 * comment for what is deliberately not reproduced here).
 *
 * The page header already names the pack, so this tab starts at what the
 * header cannot say: the installed version, a drop-down of the others with
 * an Update button, and -- only once there is a version to read about --
 * its changelog.
 */
Item {
    id: root

    // InstanceDetails.managedPack (ManagedPackController), or null.
    property var controller: null
    // The TaskWatcher of an update in progress, if any.
    property var watcher: null
    readonly property bool updating: !!root.watcher && root.watcher.running

    readonly property var versions: root.controller ? root.controller.versions : []
    readonly property bool hasVersions: !!root.controller && root.controller.hasPackId && root.versions.length > 0
    // The version the drop-down shows, or null.
    readonly property var picked: root.hasVersions && versionBox.currentIndex >= 0
                                  && versionBox.currentIndex < root.versions.length
                                  ? root.versions[versionBox.currentIndex] : null
    // One line per version for the drop-down: its name, the Minecraft it is
    // for, and a word where it is not a plain release or is the installed one.
    readonly property var versionLabels: root.versions.map((v) => {
        var label = String(v.name || v.label)
        if (v.minecraft)
            label = qsTr("%1, Minecraft %2").arg(label).arg(v.minecraft)
        if (v.releaseType === "alpha")
            label = qsTr("%1, alpha").arg(label)
        else if (v.releaseType === "beta")
            label = qsTr("%1, beta").arg(label)
        return v.current ? qsTr("%1 (installed)").arg(label) : label
    })
    readonly property int versionBoxWidth: 360

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

        TabToolbar {
            Layout.fillWidth: true

            leading: [
                Tag { text: root.controller ? root.controller.providerLabel : "" },
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: root.controller && root.controller.installedVersionName.length > 0
                          ? qsTr("Installed %1").arg(root.controller.installedVersionName)
                          : qsTr("Installed version unknown")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.body.pixelSize
                }
            ]

            Button {
                text: qsTr("Website")
                icon.source: Icons.url("external-link")
                visible: root.controller && root.controller.packUrl.length > 0
                onClicked: Qt.openUrlExternally(root.controller.packUrl)
            }
            // An update from a pack file on this computer: works for any
            // pack, including one with no catalogue id.
            HoverReason {
                reason: root.controller && root.controller.running
                        ? qsTr("The game is running. The pack can be updated once it closes.") : ""
                Button {
                    enabled: !root.updating && !!root.controller && !root.controller.running
                    text: qsTr("Update from file…")
                    icon.source: Icons.url("archive")
                    onClicked: fileDialog.open()
                }
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
                text: qsTr("MeshMC does not have a catalogue id on record for this pack, so it cannot list other versions here. You can still update it from a pack file, or install the pack again from Discover for a specific version.")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.controller && root.controller.hasPackId && root.controller.error.length > 0
            spacing: Theme.space.sm
            // What went wrong in plain words only: the catalogue's own
            // message is a raw job string meant for the log, not the user.
            Text {
                Layout.fillWidth: true
                text: qsTr("Could not load the versions of this pack. Check your connection, then reload.")
                color: Theme.palette.danger
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            Button {
                text: qsTr("Reload")
                icon.source: Icons.url("refresh")
                onClicked: root.controller.reload()
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.hasPackId && root.controller.error.length === 0
                     && !root.controller.loading && root.versions.length === 0
            wrapMode: Text.Wrap
            text: qsTr("The catalogue lists no other versions of this pack.")
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
        }

        // The other versions, as a drop-down: the newest is picked at first,
        // so its changelog is there to read before updating to it.
        RowLayout {
            Layout.fillWidth: true
            visible: root.hasVersions
            spacing: Theme.space.sm

            Text {
                text: qsTr("Update to")
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            ComboBox {
                id: versionBox
                Layout.preferredWidth: root.versionBoxWidth
                model: root.versionLabels
                Accessible.name: qsTr("Version")
            }
            Item { Layout.fillWidth: true }
            Button {
                enabled: !root.updating && !!root.picked && root.picked.installable && !root.picked.current
                // The tab's one main action, shown as such only while it
                // would do something: a version other than the installed
                // one is picked (the newest, by default, when there is
                // an update).
                highlighted: enabled
                text: qsTr("Update")
                icon.source: Icons.url("download")
                onClicked: root.watcher = root.controller.updateToVersion(versionBox.currentIndex)
            }
        }

        Rectangle {
            id: changelogHost
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.hasVersions && !!root.picked
            radius: Theme.radius.lg
            color: Theme.palette.surfaceSunken
            border.width: 1
            border.color: Theme.palette.border

            Flickable {
                anchors.fill: parent
                anchors.margins: Theme.space.md
                contentWidth: width
                contentHeight: changelog.implicitHeight
                clip: true
                ScrollBar.vertical: ScrollBar {}

                // A changelog may link images; see RemoteRichText.
                RemoteRichText {
                    id: changelog
                    width: parent.width
                    source: !root.picked ? ""
                            : root.picked.changelog.length > 0
                            ? root.picked.changelog
                            : qsTr("No changelog available for this version.")
                    onLinkActivated: (link) => root.openChangelogLink(link)
                }
            }
        }

        // Pushes the rows above to the top when there is no changelog box.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !changelogHost.visible
        }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Choose the pack file")
        nameFilters: root.controller ? [root.controller.fileFilter] : []
        onAccepted: root.watcher = root.controller.updateFromFile(selectedFile)
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
