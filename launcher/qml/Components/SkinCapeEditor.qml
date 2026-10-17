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
 *  - the game's nine default skins under the user's own, as the official
 *    launcher offers them (SkinsController.defaultSkins, downloaded on
 *    first use): picked, previewed and worn the same way, the arm width
 *    choosing which of its two textures;
 *  - the account's capes.
 * Uploads go through AccountsController (changeSkin/resetSkin/changeCape),
 * the same services the widget dialog used.
 *
 * One instance is shared by the whole page (see AccountsPage.qml); `row` is
 * set right before open() for whichever account was clicked. In a short
 * window the body scrolls, and the × in the title row stays on screen
 * however many capes the account owns. No footer: a Close row of its own
 * cost the height that left the capes under the body's scroll fade.
 */
Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    // The tallest the dialog may be: the window, less a margin.
    readonly property real maxHeight: parent ? parent.height - Theme.space.xxl * 2 : implicitHeight
    height: Math.min(implicitHeight, root.maxHeight)
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
    // turn), the library box (a row of the user's tiles and a row of the
    // default ones), and one skin tile.
    readonly property int preferredWidth: 840
    readonly property int previewWidth: 220
    readonly property int previewHeight: 300
    readonly property int libraryHeight: 320
    // The library box's height: libraryHeight at least, and on a taller
    // window as tall as every tile needs, so the box scrolls (and cuts a
    // row) only where the window is short. Worked out a step later, never
    // inside a layout pass: the room depends on the dialog's own height,
    // which depends on the box.
    property real libraryRoom: libraryHeight
    function fitLibrary() {
        const needed = skinGrid.contentHeight + skinGrid.anchors.margins * 2
        const room = root.maxHeight - (root.implicitHeight - libraryBox.height)
        root.libraryRoom = Math.max(root.libraryHeight, Math.min(needed, room))
    }
    onMaxHeightChanged: Qt.callLater(root.fitLibrary)
    onImplicitHeightChanged: Qt.callLater(root.fitLibrary)
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
    readonly property var defaultList: root.skins ? root.skins.defaultSkins : []
    // The skin picked for the preview and "Wear": a library skin by name,
    // or a default one by name with the arm width it is shown with. Both
    // empty shows what the account wears now.
    property string selectedName: ""
    property string selectedDefault: ""
    property bool defaultSlim: false
    readonly property var selected: {
        if (root.selectedDefault.length > 0) {
            for (var d = 0; d < root.defaultList.length; ++d)
                if (root.defaultList[d].name === root.selectedDefault)
                    return root.defaultList[d].variants[root.defaultSlim ? 1 : 0]
            return null
        }
        for (var i = 0; i < root.skinList.length; ++i)
            if (root.skinList[i].name === root.selectedName)
                return root.skinList[i]
        return null
    }
    readonly property bool selectedReady: !!root.selected && root.selected.ready !== false
    function selectLibrary(name) {
        root.selectedDefault = ""
        root.selectedName = root.selectedName === name ? "" : name
    }
    function selectDefault(character) {
        root.selectedName = ""
        if (root.selectedDefault === character.name) {
            root.selectedDefault = ""
            return
        }
        root.defaultSlim = character.slim
        root.selectedDefault = character.name
    }
    // Whether any default skin is here, and whether more are on the way.
    readonly property bool defaultsLoading: !!root.skins && root.skins.defaultSkinsLoading
    readonly property bool anyDefaultReady: {
        for (var i = 0; i < root.defaultList.length; ++i)
            if (root.defaultList[i].variants[0].ready || root.defaultList[i].variants[1].ready)
                return true
        return false
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
        root.selectedDefault = ""
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
        // "accounts-demo=default": Alex from the default skins, scrolled to.
        if (pose === "default" && root.defaultList.length > 1) {
            root.selectDefault(root.defaultList[1])
            Qt.callLater(skinGrid.positionViewAtEnd)
        }
    }

    Connections {
        target: root.controller ? root.controller.accounts : null
        function onDataChanged() { root.reload() }
    }
    // An account without a skin of its own shows the game's default once
    // that has downloaded (see AccountsController::accountSkinInfo()).
    Connections {
        target: root.controller
        function onDefaultSkinsRevisionChanged() { root.reload() }
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
        if (!root.selectedReady)
            return
        // A default skin is not in the library, so there is nothing to
        // mark: the account's own refresh says it is worn.
        var fromLibrary = root.selectedDefault.length === 0
        root.run(root.controller.changeSkin(root.row, skin.path, skin.slim),
                 qsTr("The skin could not be uploaded."),
                 function(ok) {
                     if (ok && fromLibrary && root.skins)
                         root.skins.markWorn(skin.name)
                 })
    }

    // A section's name inside the skin library box.
    component LibrarySectionLabel: Text {
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
        font.weight: Theme.type.label.weight
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
                            id: previewStage
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.previewHeight
                            radius: Theme.radius.lg
                            // A lit stage, not a sunken well, so a dark skin
                            // stands off it (SkinPreview adds the glow behind
                            // the player), lighter at the top than at its
                            // floor. In the dark theme its top is lighter than
                            // the dialog around it, `lift` of the way to the
                            // text colour; in the light one it is the sunken
                            // grey, the floor `floorShade` darker.
                            readonly property real lift: 0.08
                            readonly property real floorShade: 0.05
                            gradient: Gradient {
                                GradientStop {
                                    position: 0
                                    color: Theme.dark ? Qt.tint(Theme.palette.surfaceOverlay,
                                                                Qt.alpha(Theme.palette.textPrimary, previewStage.lift))
                                                      : Theme.palette.surfaceSunken
                                }
                                GradientStop {
                                    position: 1
                                    color: Theme.dark ? Theme.palette.surfaceRaised
                                                      : Qt.tint(Theme.palette.surfaceSunken,
                                                                Qt.alpha(Theme.palette.textPrimary, previewStage.floorShade))
                                }
                            }

                            SkinPreview {
                                id: preview
                                anchors.fill: parent
                                anchors.margins: Theme.space.md
                                catAvailable: root.catAvailable
                                skinTexture: root.selected ? root.selected.textureUrl || ""
                                             : root.info.hasSkin && root.accountId.length > 0
                                               ? "image://accountface/skin/" + root.accountId + "?rev=" + root.rev : ""
                                flatSource: root.selected ? root.selected.bodyUrl || ""
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
                                text: qsTr("Skins")
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
                            id: libraryBox
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.libraryRoom
                            radius: Theme.radius.lg
                            color: Theme.palette.surfaceSunken

                            // The user's skins, then the game's default ones
                            // in the same scrolling box, as two sections.
                            GridView {
                                id: skinGrid
                                onContentHeightChanged: Qt.callLater(root.fitLibrary)
                                anchors.fill: parent
                                anchors.margins: Theme.space.sm
                                clip: true
                                cellWidth: root.tileWidth + Theme.space.sm
                                cellHeight: root.tileHeight + Theme.space.sm
                                model: root.skinList
                                ScrollBar.vertical: ScrollBar {}
                                header: Column {
                                    width: skinGrid.width
                                    bottomPadding: Theme.space.sm
                                    spacing: Theme.space.sm
                                    LibrarySectionLabel { text: qsTr("Your skins") }
                                    Text {
                                        width: parent.width
                                        visible: root.skinList.length === 0
                                        text: qsTr("No skins here yet. Add one from a file, a link or a player's name.")
                                        wrapMode: Text.Wrap
                                        color: Theme.palette.textTertiary
                                        font.family: Theme.font.family
                                        font.pixelSize: Theme.type.label.pixelSize
                                    }
                                }
                                delegate: SkinTile {
                                    required property var modelData
                                    width: root.tileWidth
                                    height: root.tileHeight
                                    spriteSize: root.spriteSize
                                    name: modelData.name
                                    spriteUrl: modelData.thumbnailUrl
                                    worn: modelData.worn
                                    selected: root.selectedDefault.length === 0 && root.selectedName === modelData.name
                                    menuEnabled: true
                                    onClicked: root.selectLibrary(modelData.name)
                                    onMenuRequested: skinMenu.openFor(modelData.name, modelData.worn)
                                }
                                footer: Column {
                                    width: skinGrid.width
                                    topPadding: Theme.space.md
                                    spacing: Theme.space.sm
                                    Row {
                                        spacing: Theme.space.sm
                                        LibrarySectionLabel {
                                            id: defaultsLabel
                                            text: qsTr("Default skins")
                                        }
                                        Text {
                                            anchors.baseline: defaultsLabel.baseline
                                            visible: root.defaultsLoading || !root.anyDefaultReady
                                            text: root.defaultsLoading ? qsTr("Downloading…")
                                                                       : qsTr("They download when you are online.")
                                            color: Theme.palette.textTertiary
                                            font.family: Theme.font.family
                                            font.pixelSize: Theme.type.caption.pixelSize
                                        }
                                    }
                                    Flow {
                                        width: parent.width
                                        spacing: Theme.space.sm
                                        Repeater {
                                            model: root.defaultList
                                            delegate: SkinTile {
                                                id: defaultTile
                                                required property var modelData
                                                readonly property bool isSelected: root.selectedDefault === modelData.name
                                                // Drawn with its own arm width, or the one
                                                // picked for it while selected.
                                                readonly property var shown: modelData.variants[(defaultTile.isSelected ? root.defaultSlim : modelData.slim) ? 1 : 0]
                                                width: root.tileWidth
                                                height: root.tileHeight
                                                spriteSize: root.spriteSize
                                                name: modelData.name
                                                spriteUrl: defaultTile.shown.thumbnailUrl || ""
                                                ready: defaultTile.shown.ready
                                                worn: modelData.variants[0].worn || modelData.variants[1].worn
                                                selected: defaultTile.isSelected
                                                onClicked: root.selectDefault(modelData)
                                            }
                                        }
                                    }
                                }
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
                            // Two radio buttons, as in the official
                            // launcher's skin editor -- exactly one of two,
                            // not a pill switch.
                            Repeater {
                                model: [
                                    { slim: false, label: qsTr("Classic") },
                                    { slim: true, label: qsTr("Slim") }
                                ]
                                delegate: RadioButton {
                                    id: armsRadio
                                    required property var modelData
                                    readonly property bool current: (root.selected ? root.selected.slim : root.info.slim) === modelData.slim
                                    enabled: !!root.selected && !root.busy
                                    text: modelData.label
                                    // Checked follows the skin, not the
                                    // click: a click only asks for the change,
                                    // and the binding (which a click breaks)
                                    // is put back each time.
                                    autoExclusive: false
                                    checked: armsRadio.current
                                    onClicked: {
                                        // A default skin swaps to its other
                                        // texture; a library one records it.
                                        if (!armsRadio.current) {
                                            if (root.selectedDefault.length > 0)
                                                root.defaultSlim = modelData.slim
                                            else
                                                root.skins.setSlim(root.selectedName, modelData.slim)
                                        }
                                        armsRadio.checked = Qt.binding(() => armsRadio.current)
                                    }
                                }
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
                                enabled: root.selectedReady && !root.busy
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

    header: Item {
        implicitHeight: titleRow.implicitHeight
        DialogHeader {
            id: titleRow
            anchors.left: parent.left
            anchors.right: closeButton.left
            title: root.title
        }
        IconButton {
            id: closeButton
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.md
            // On the title's line: DialogHeader's label sits below its
            // own top padding.
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: Theme.space.lg / 2
            iconName: "x"
            tip: qsTr("Close")
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
