// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * The instance's installed content: mods, resource packs, shader packs and
 * -- on old enough instances -- texture packs, one PackList reused across
 * all four kinds. Locked while the game runs, same as the old Mods tab was:
 * the files are open, and a change would only apply to the next launch.
 */
Item {
    id: root

    // InstanceDetails: mods/resourcePacks/shaderPacks/texturePacks (+ *Dir),
    // isMinecraft, contentChangesAllowed, setEnabled, remove, install.
    property var details: null
    property string kind: "mods"
    readonly property bool unlocked: !!details && details.contentChangesAllowed
    readonly property int count: packList.count
    signal openFolderRequested(string path)
    signal browseRequested()

    readonly property bool isMinecraft: !!details && details.isMinecraft

    // One entry per content kind this instance actually has; texture packs
    // only show up on instances old enough to use them (InstanceDetails
    // already hides that model behind a null texturePacks otherwise).
    readonly property var kinds: {
        var list = [
            { value: "mods", label: qsTr("Mods"), icon: "package",
              model: root.details ? root.details.mods : null,
              dir: root.details ? root.details.modsDir : "",
              emptyTitle: qsTr("No mods"),
              emptyBody: qsTr("Drop .jar files into the mods folder, add one from a file, or install a modpack from Discover.") },
            { value: "resourcepacks", label: qsTr("Resource packs"), icon: "image",
              model: root.details ? root.details.resourcePacks : null,
              dir: root.details ? root.details.resourcePacksDir : "",
              emptyTitle: qsTr("No resource packs"),
              emptyBody: qsTr("Change how the game looks -- add one from a file, or browse for one.") },
            { value: "shaderpacks", label: qsTr("Shader packs"), icon: "layers",
              model: root.details ? root.details.shaderPacks : null,
              dir: root.details ? root.details.shaderPacksDir : "",
              emptyTitle: qsTr("No shader packs"),
              emptyBody: qsTr("Needs a shader-capable renderer, such as Iris or OptiFine, to have any effect.") }
        ]
        if (root.details && root.details.texturePacks) {
            list.push({ value: "texturepacks", label: qsTr("Texture packs"), icon: "grid",
                        model: root.details.texturePacks,
                        dir: root.details.texturePacksDir,
                        emptyTitle: qsTr("No texture packs"),
                        emptyBody: qsTr("This version predates resource packs and uses texture packs instead.") })
        }
        return list
    }

    readonly property var current: {
        for (var i = 0; i < kinds.length; ++i) {
            if (kinds[i].value === root.kind)
                return kinds[i]
        }
        return kinds.length > 0 ? kinds[0] : null
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md
        visible: root.isMinecraft

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            SegmentedControl {
                options: root.kinds.map((k) => ({ value: k.value, label: k.label }))
                current: root.kind
                onActivated: (value) => root.kind = value
            }

            Text {
                Layout.fillWidth: true
                text: root.unlocked ? "" : qsTr("The game is running; content can be changed once it has closed.")
                color: Theme.palette.warning
                elide: Text.ElideRight
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }

            Button {
                flat: true
                text: qsTr("Browse online")
                onClicked: root.browseRequested()
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.current ? root.current.dir : "")
            }
            Button {
                highlighted: true
                enabled: root.unlocked
                text: qsTr("Add from file…")
                icon.source: Icons.url("plus")
                onClicked: fileDialog.open()
            }
        }

        PackList {
            id: packList
            Layout.fillWidth: true
            Layout.fillHeight: true
            details: root.details
            kind: root.kind
            model: root.current ? root.current.model : null
            unlocked: root.unlocked
            iconName: root.current ? root.current.icon : "package"
        }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: root.isMinecraft && packList.count === 0
        title: root.current ? root.current.emptyTitle : ""
        body: root.current ? root.current.emptyBody : ""
        actionText: qsTr("Add from file…")
        onActionTriggered: fileDialog.open()
        MeshIcon { iconName: root.current ? root.current.icon : "package"; size: 40; color: Theme.palette.textTertiary }
    }

    EmptyState {
        anchors.centerIn: parent
        visible: !root.isMinecraft
        title: qsTr("No content")
        body: qsTr("This instance cannot have mods, resource packs or shader packs.")
        MeshIcon { iconName: "package"; size: 40; color: Theme.palette.textTertiary }
    }

    FileDialog {
        id: fileDialog
        title: root.current ? qsTr("Add %1").arg(root.current.label.toLowerCase()) : qsTr("Add content")
        nameFilters: root.kind === "mods" ? [qsTr("Mod files (*.jar *.zip *.litemod)"), qsTr("All files (*)")]
                                          : [qsTr("Pack files (*.zip)"), qsTr("All files (*)")]
        onAccepted: if (root.details) root.details.install(root.kind, selectedFile.toString())
    }
}
