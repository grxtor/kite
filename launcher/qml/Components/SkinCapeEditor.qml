// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Skins and capes for one Microsoft account, opened from AccountsPage --
 * the QML counterpart of the widget SkinManageDialog:
 *  - a preview of the player (SkinPreview: 3D where Qt Quick3D draws, with
 *    the cape or the elytra on its back; the flat view elsewhere);
 *  - the skin library (AccountsController.skins(row), a SkinsController
 *    over the launcher's skins folder): add a skin from a file, a link or
 *    another player's name, rename, delete, open the folder, set the arm
 *    width, and wear the selected one;
 *  - the account's capes.
 * Uploads go through AccountsController (changeSkin/resetSkin/changeCape),
 * the same services the widget dialog used.
 *
 * One instance is shared by the whole page (see AccountsPage.qml); `row` is
 * set right before open() for whichever account was clicked. In a short
 * window the body scrolls, so Close stays on screen however many capes
 * the account owns.
 */
Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : implicitHeight)
    modal: true
    // Worded like the buttons that open it ("Skin and cape"), with the
    // player's name as part of the sentence rather than after a dash.
    title: root.accountName.length > 0 ? qsTr("Skin and cape for %1").arg(root.accountName)
                                        : qsTr("Skin and cape")

    // AccountsController: accountSkinInfo, skins, changeSkin, resetSkin,
    // changeCape.
    property var controller: null
    property int row: -1
    property string accountId: ""
    property string accountName: ""
    // Bumped by AccountsPage whenever an account's data may have changed,
    // so image urls change and QML actually refetches them -- see
    // AccountsPage.qml's imageRevision.
    property int rev: 0
    // Whether launcher/qml/Cat (Qt Quick3D) exists -- see SkinPreview.
    property bool catAvailable: false

    // Sizes: the dialog, the preview column (a 1:2 player plus room to
    // turn), the library box (two rows of tiles), and one skin tile.
    readonly property int preferredWidth: 840
    readonly property int previewWidth: 220
    readonly property int previewHeight: 300
    readonly property int libraryHeight: 260
    readonly property int tileWidth: 96
    readonly property int tileHeight: 120
    readonly property int spriteSize: 72

    // Snapshot from controller.accountSkinInfo(row) -- re-read on open and
    // whenever the account list reports a change (a refresh landing).
    property var info: emptyInfo()
    function emptyInfo() {
        return { valid: false, slim: false, hasSkin: false, currentCapeId: "", currentCapeUrl: "", capes: [] }
    }
    function reload() {
        // Not gated on row >= 0: row -1 is the qml-preview-tools demo
        // sentinel (see AccountsController::skinDemoRequested()).
        root.info = root.controller
                    ? root.controller.accountSkinInfo(root.row) : root.emptyInfo()
    }

    // The account's skin library; null until opened on a real row.
    property var skins: null
    readonly property var skinList: root.skins ? root.skins.skins : []
    // The library skin picked for the preview and "Wear", by name; empty
    // shows what the account wears now.
    property string selectedName: ""
    readonly property var selected: {
        for (var i = 0; i < root.skinList.length; ++i)
            if (root.skinList[i].name === root.selectedName)
                return root.skinList[i]
        return null
    }

    // The cape on the preview: the account's current one.
    readonly property string capeUrl: root.info.currentCapeUrl || ""
    property bool showElytra: false

    // Whichever change is running, if any -- shared by upload/reset/cape
    // and the imports, so only one runs at a time and its status/error
    // show the same way.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    property string message: ""
    property bool messageIsError: false

    function say(text, isError) {
        root.message = text
        root.messageIsError = !!isError
    }

    onOpened: {
        reload()
        root.skins = root.controller ? root.controller.skins(root.row) : null
        // The review demo (row -1) shows its first skin straight away.
        root.selectedName = root.row === -1 && root.skinList.length > 0 ? root.skinList[0].name : ""
        root.watcher = null
        root.say("", false)
        root.showElytra = false
        preview.showFront()
        // Review renders: "accounts-demo=cape" or "=elytra" open on the
        // player's back (see AccountsController::skinDemoPose()).
        var pose = root.row === -1 && root.controller ? root.controller.skinDemoPose : ""
        if (pose === "cape" || pose === "elytra") {
            root.showElytra = pose === "elytra"
            Qt.callLater(preview.showBack)
        }
    }

    Connections {
        target: root.controller ? root.controller.accounts : null
        function onDataChanged() { root.reload() }
    }

    // Starts @p w; @p done(ok) runs once it ends.
    function run(w, failText, done) {
        if (!w) {
            root.say(failText, true)
            return
        }
        root.say("", false)
        root.watcher = w
        w.finished.connect(function(ok) {
            root.reload()
            if (!ok)
                root.say(w.error.length > 0 ? w.error : failText, true)
            if (done)
                done(ok)
        })
    }

    // Puts cape @p capeId on (empty takes it off), and turns the player
    // round to show it once it is on.
    function changeCape(capeId) {
        root.run(root.controller.changeCape(root.row, capeId), qsTr("The cape could not be changed."),
                 function(ok) {
                     if (ok && capeId.length > 0)
                         preview.showBack()
                 })
    }

    function wearSelected() {
        var skin = root.selected
        if (!skin)
            return
        root.run(root.controller.changeSkin(root.row, skin.path, skin.slim),
                 qsTr("The skin could not be uploaded."),
                 function(ok) {
                     if (ok && root.skins)
                         root.skins.markWorn(skin.name)
                 })
    }

    // One skin in the library: its sprite, its name, and a mark on the one
    // the account wears. Click selects it; right click, or the ⋯ that shows
    // on hover (over the sprite, so only while pointed at), opens actions.
    component SkinTile: AbstractButton {
        id: tile
        required property var modelData
        readonly property bool isSelected: root.selectedName === tile.modelData.name

        implicitWidth: root.tileWidth
        implicitHeight: root.tileHeight
        hoverEnabled: true
        Accessible.role: Accessible.RadioButton
        Accessible.name: tile.modelData.name
        Accessible.checked: tile.isSelected
        onClicked: root.selectedName = tile.isSelected ? "" : tile.modelData.name

        background: Rectangle {
            radius: Theme.radius.md
            color: tile.isSelected ? Theme.palette.accentSubtle
                 : tile.hovered ? Theme.palette.hoverOverlay : "transparent"
            border.width: 1
            border.color: tile.isSelected ? Theme.palette.accent : Theme.palette.border
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        }
        contentItem: Item {
            Image {
                id: sprite
                anchors.top: parent.top
                anchors.topMargin: Theme.space.sm
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.spriteSize
                height: root.spriteSize
                source: tile.modelData.thumbnailUrl
                sourceSize: Qt.size(root.spriteSize, root.spriteSize)
                smooth: false
                fillMode: Image.PreserveAspectFit
            }
            Text {
                anchors.top: sprite.bottom
                anchors.topMargin: Theme.space.xs
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.space.xs
                horizontalAlignment: Text.AlignHCenter
                text: tile.modelData.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
            MeshIcon {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Theme.space.xs
                visible: tile.modelData.worn
                iconName: "check"
                size: Theme.icon.sm
                color: Theme.palette.accent
            }
            IconButton {
                id: moreButton
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.space.xxs
                size: Theme.control.heightSm
                visible: tile.hovered || moreButton.hovered
                iconName: "more"
                tip: qsTr("More")
                onClicked: skinMenu.openFor(tile.modelData.name, tile.modelData.worn)
            }
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: skinMenu.openFor(tile.modelData.name, tile.modelData.worn)
        }
    }

    contentItem: Item {
        implicitWidth: body.implicitWidth
        implicitHeight: body.implicitHeight

        Flickable {
            id: scroller
            anchors.fill: parent
            contentWidth: width
            contentHeight: body.implicitHeight
            // Only when it does not fit, so a drag on the player turns it.
            interactive: contentHeight > height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: body
                width: scroller.width
                spacing: Theme.space.lg

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.xl

                    // -- The preview -------------------------------------------
                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: root.previewWidth
                        spacing: Theme.space.sm

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.previewHeight
                            radius: Theme.radius.lg
                            color: Theme.palette.surfaceSunken

                            SkinPreview {
                                id: preview
                                anchors.fill: parent
                                anchors.margins: Theme.space.md
                                catAvailable: root.catAvailable
                                skinTexture: root.selected ? root.selected.textureUrl
                                             : root.info.hasSkin && root.accountId.length > 0
                                               ? "image://accountface/skin/" + root.accountId + "?rev=" + root.rev : ""
                                flatSource: root.selected ? root.selected.bodyUrl
                                            : root.accountId.length > 0
                                              ? "image://accountface/body/" + root.accountId + "?rev=" + root.rev : ""
                                slim: root.selected ? root.selected.slim : root.info.slim
                                capeTexture: root.capeUrl
                                elytra: root.showElytra
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: preview.threeD
                            text: qsTr("Drag to turn it around.")
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                        CheckBox {
                            visible: preview.threeD
                            text: qsTr("Show the elytra")
                            checked: root.showElytra
                            onToggled: {
                                root.showElytra = checked
                                preview.showBack()
                            }
                        }
                    }

                    // -- The library -------------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.space.md

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.sm
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("Your skins")
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.title.pixelSize
                                font.weight: Theme.type.title.weight
                            }
                            Button {
                                id: addButton
                                text: qsTr("Add skin")
                                icon.source: Icons.url("plus")
                                enabled: !!root.skins && !root.busy
                                onClicked: addMenu.popup(addButton, 0, addButton.height + Theme.space.xs)
                            }
                            IconButton {
                                iconName: "folder"
                                tip: qsTr("Open the skins folder")
                                flat: false
                                enabled: !!root.skins
                                onClicked: root.skins.openFolder()
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.libraryHeight
                            radius: Theme.radius.lg
                            color: Theme.palette.surfaceSunken

                            Text {
                                anchors.centerIn: parent
                                width: parent.width - Theme.space.xl * 2
                                visible: root.skinList.length === 0
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("No skins here yet. Add one from a file, a link or a player's name.")
                                wrapMode: Text.Wrap
                                color: Theme.palette.textTertiary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }

                            GridView {
                                id: skinGrid
                                anchors.fill: parent
                                anchors.margins: Theme.space.sm
                                clip: true
                                cellWidth: root.tileWidth + Theme.space.sm
                                cellHeight: root.tileHeight + Theme.space.sm
                                model: root.skinList
                                ScrollBar.vertical: ScrollBar {}
                                delegate: SkinTile {}
                            }
                            ScrollFade {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: Theme.space.sm
                                flickable: skinGrid
                                fadeTo: Theme.palette.surfaceSunken
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.sm
                            Text {
                                text: qsTr("Arms")
                                color: Theme.palette.textSecondary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }
                            SegmentedControl {
                                enabled: !!root.selected && !root.busy
                                options: [
                                    { value: "classic", label: qsTr("Classic") },
                                    { value: "slim", label: qsTr("Slim") }
                                ]
                                current: (root.selected ? root.selected.slim : root.info.slim) ? "slim" : "classic"
                                onActivated: (value) => root.skins.setSlim(root.selectedName, value === "slim")
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                flat: true
                                text: qsTr("Reset skin")
                                enabled: !root.busy
                                onClicked: root.run(root.controller.resetSkin(root.row), qsTr("The skin could not be reset."))
                            }
                            Button {
                                highlighted: true
                                text: qsTr("Wear this skin")
                                enabled: !!root.selected && !root.busy
                                onClicked: root.wearSelected()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: root.busy || root.message.length > 0
                            spacing: Theme.space.sm
                            BusyIndicator {
                                visible: root.busy
                                running: visible
                                Layout.preferredWidth: Theme.icon.md
                                Layout.preferredHeight: Theme.icon.md
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: root.messageIsError && !root.busy ? Theme.palette.danger : Theme.palette.textTertiary
                                text: root.busy ? (root.watcher.status.length > 0 ? root.watcher.status : qsTr("Working…"))
                                                : root.message
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.label.pixelSize
                            }
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.palette.divider }

                Text {
                    text: qsTr("Capes")
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm

                    CapeTile {
                        label: qsTr("No cape")
                        isNoCapeOption: true
                        selected: root.info.currentCapeId.length === 0
                        enabled: !root.busy
                        onClicked: root.changeCape("")
                    }
                    Repeater {
                        model: root.info.capes
                        delegate: CapeTile {
                            required property var modelData
                            label: modelData.alias
                            capeUrl: modelData.url
                            selected: modelData.id === root.info.currentCapeId
                            enabled: !root.busy
                            onClicked: root.changeCape(modelData.id)
                        }
                    }
                }
            }
        }
        ScrollFade {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            flickable: scroller
            fadeTo: Theme.palette.surfaceOverlay
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0
        Button {
            text: qsTr("Close")
            onClicked: root.close()
        }
    }

    // -- Adding, renaming and deleting skins -------------------------------
    Menu {
        id: addMenu
        MenuItem {
            text: qsTr("From a file…")
            onTriggered: skinFileDialog.open()
        }
        MenuItem {
            text: qsTr("From a link…")
            onTriggered: {
                linkDialog.value = ""
                linkDialog.error = ""
                linkDialog.open()
            }
        }
        MenuItem {
            text: qsTr("From a player's name…")
            onTriggered: {
                playerDialog.value = ""
                playerDialog.error = ""
                playerDialog.open()
            }
        }
    }

    Menu {
        id: skinMenu
        property string targetName
        property bool targetWorn: false
        function openFor(name, worn) {
            targetName = name
            targetWorn = worn
            popup()
        }
        MenuItem {
            text: qsTr("Rename…")
            icon.source: Icons.url("edit")
            onTriggered: {
                renameDialog.targetName = skinMenu.targetName
                renameDialog.value = skinMenu.targetName
                renameDialog.error = ""
                renameDialog.open()
            }
        }
        MenuItem {
            text: qsTr("Delete…")
            icon.source: Icons.url("trash")
            enabled: !skinMenu.targetWorn
            onTriggered: {
                deleteDialog.targetName = skinMenu.targetName
                deleteDialog.open()
            }
        }
    }

    // As the widget dialog did: asked first, then to the trash where there
    // is one.
    ConfirmDialog {
        id: deleteDialog
        property string targetName
        title: qsTr("Delete skin")
        text: qsTr("Delete “%1” from your skins?").arg(deleteDialog.targetName)
        confirmText: qsTr("Delete skin")
        onConfirmed: {
            var problem = root.skins.remove(deleteDialog.targetName)
            if (problem.length > 0)
                root.say(problem, true)
            else if (root.selectedName === deleteDialog.targetName)
                root.selectedName = ""
        }
    }

    FileDialog {
        id: skinFileDialog
        title: qsTr("Add a skin")
        nameFilters: [qsTr("PNG images (*.png)")]
        onAccepted: {
            var problem = root.skins.importFile(selectedFile.toString())
            root.say(problem.length > 0 ? problem : qsTr("Skin added."), problem.length > 0)
        }
    }

    PromptDialog {
        id: linkDialog
        title: qsTr("Add a skin from a link")
        label: qsTr("A direct link to the skin's PNG file.")
        placeholder: "https://"
        confirmText: qsTr("Add")
        onSubmitted: (text) => {
            var w = root.skins.importFromUrl(text)
            if (!w) {
                error = qsTr("That is not a link. Links start with https://")
                return
            }
            close()
            root.run(w, qsTr("The skin could not be added."),
                     function(ok) { if (ok) root.say(qsTr("Skin added."), false) })
        }
    }

    PromptDialog {
        id: playerDialog
        title: qsTr("Add another player's skin")
        label: qsTr("The Minecraft name of the player whose skin you want.")
        confirmText: qsTr("Add")
        onSubmitted: (text) => {
            var w = root.skins.importFromUsername(text)
            if (!w) {
                error = qsTr("Type a player's name.")
                return
            }
            close()
            root.run(w, qsTr("The skin could not be added."),
                     function(ok) { if (ok) root.say(qsTr("Skin added."), false) })
        }
    }

    PromptDialog {
        id: renameDialog
        property string targetName
        title: qsTr("Rename skin")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            if (root.skins.rename(targetName, text)) {
                if (root.selectedName === targetName)
                    root.selectedName = text.trim()
                close()
            } else {
                error = qsTr("That name is empty or already taken.")
            }
        }
    }
}
