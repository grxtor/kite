// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The instance's screenshots as a grid of thumbnails, newest first. Ticking a
 * hovered tile selects it; once anything is selected, clicking a tile toggles
 * it. Upload goes to Imgur (a link, or an album for several) and is left out
 * of builds without an Imgur client id.
 */
Item {
    id: root

    // ScreenshotListModel: name, path, url, modified, size; remove(),
    // rename(), copyImage(), pathAt().
    property var model: null
    property string directory
    property var details: null
    readonly property var uploader: details ? details.screenshotUploader : null
    readonly property bool canUpload: !!uploader && uploader.available
    signal openFolderRequested(string path)

    readonly property int minTileWidth: 240
    readonly property int gap: Theme.space.md
    readonly property int captionHeight: Theme.space.xs + Math.ceil(captionMetrics.height)

    // The game names screenshots after the moment they were taken ("2026-09-22_18.00.12.png"),
    // shown as a date and time; a renamed one keeps its name. Never the extension.
    function captionOf(name) {
        var bare = root.baseName(name)
        var m = /^(\d{4})-(\d{2})-(\d{2})_(\d{2})\.(\d{2})\.(\d{2})$/.exec(bare)
        if (!m)
            return bare
        var taken = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]), Number(m[4]), Number(m[5]), Number(m[6]))
        return qsTr("%1, %2").arg(Format.calendarDate(taken)).arg(taken.toLocaleTimeString(Qt.locale(), Locale.ShortFormat))
    }

    // Selected screenshots by path, {path: true}; paths, not rows, because rows move when files change.
    property var selected: ({})
    property int selectedCount: 0
    // Set by the Upload actions, so only this tab reacts to the result.
    property bool uploading: false

    // For the "review=<name>" dev route: everything selected, or the first picture's rename.
    function review(name) {
        if (name === "shotselect") {
            root.selectAll()
        } else if (name === "shotrename" && root.model && root.model.count > 0) {
            const path = root.model.pathAt(0)
            renamePrompt.path = path
            renamePrompt.value = root.baseName(path.substring(path.lastIndexOf("/") + 1))
            renamePrompt.error = ""
            renamePrompt.open()
        }
    }

    function isSelected(path) {
        return root.selected[path] === true
    }

    function toggleSelected(path) {
        var next = {}
        for (var key in root.selected)
            next[key] = true
        if (next[path])
            delete next[path]
        else
            next[path] = true
        root.selected = next
        root.selectedCount = Object.keys(next).length
    }

    function clearSelection() {
        root.selected = ({})
        root.selectedCount = 0
    }

    function selectAll() {
        var next = {}
        for (var i = 0; root.model && i < root.model.count; ++i)
            next[root.model.pathAt(i)] = true
        root.selected = next
        root.selectedCount = Object.keys(next).length
    }

    function rowOf(path) {
        for (var i = 0; root.model && i < root.model.count; ++i) {
            if (root.model.pathAt(i) === path)
                return i
        }
        return -1
    }

    // The selected paths, or just @p path when nothing is selected or the path is outside the
    // selection.
    function targets(path) {
        if (root.isSelected(path))
            return Object.keys(root.selected)
        return [path]
    }

    function upload(paths) {
        if (!root.canUpload || root.uploader.busy)
            return
        root.uploading = true
        root.uploader.upload(paths)
    }

    // Moves @p paths to the trash, last row first so earlier rows keep their numbers.
    function removeAll(paths) {
        var rows = []
        for (var i = 0; i < paths.length; ++i) {
            var row = root.rowOf(paths[i])
            if (row >= 0)
                rows.push(row)
        }
        rows.sort(function (a, b) { return b - a })
        var failed = 0
        for (var j = 0; j < rows.length; ++j) {
            if (!root.model.remove(rows[j]))
                failed++
        }
        root.clearSelection()
        if (failed > 0)
            toast.show(qsTr("Some screenshots could not be deleted."), "danger")
    }

    function baseName(name) {
        var dot = name.lastIndexOf(".")
        return dot > 0 ? name.slice(0, dot) : name
    }

    Connections {
        target: root.uploader
        ignoreUnknownSignals: true
        function onUploaded(url, count) {
            if (!root.uploading)
                return
            root.uploading = false
            clipboard.copyText(url)
            toast.show(count > 1 ? qsTr("Album link copied to your clipboard.")
                                 : qsTr("Link copied to your clipboard."),
                       "success", qsTr("Open"), () => Qt.openUrlExternally(url))
        }
        function onFailed(message) {
            if (!root.uploading)
                return
            root.uploading = false
            toast.show(message, "danger")
        }
    }

    FontMetrics {
        id: captionMetrics
        font.family: Theme.font.family
        font.pixelSize: Theme.type.caption.pixelSize
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        TabToolbar {
            Layout.fillWidth: true

            leading: Text {
                visible: root.selectedCount > 0
                text: qsTr("%1 selected").arg(root.selectedCount)
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }

            Button {
                visible: root.selectedCount > 0
                text: qsTr("Clear selection")
                flat: true
                onClicked: root.clearSelection()
            }
            Button {
                visible: root.selectedCount === 0 && !!root.model && root.model.count > 1
                text: qsTr("Select all")
                flat: true
                onClicked: root.selectAll()
            }
            Button {
                visible: root.selectedCount > 0 && root.canUpload
                enabled: !!root.uploader && !root.uploader.busy
                text: root.uploading ? qsTr("Uploading…") : qsTr("Upload")
                icon.source: Icons.url("arrow-up")
                onClicked: root.upload(Object.keys(root.selected))
            }
            Button {
                visible: root.selectedCount > 0
                text: qsTr("Delete")
                icon.source: Icons.url("trash")
                onClicked: {
                    deleteConfirm.paths = Object.keys(root.selected)
                    deleteConfirm.text = deleteConfirm.paths.length === 1
                        ? qsTr("Move this screenshot to the trash?")
                        : qsTr("Move %1 screenshots to the trash?").arg(deleteConfirm.paths.length)
                    deleteConfirm.open()
                }
            }
            Button {
                text: qsTr("Open folder")
                icon.source: Icons.url("folder")
                onClicked: root.openFolderRequested(root.directory)
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            // Each cell carries a gap on its right; the grid is a gap wider than the page so the
            // last column's gap falls outside and the thumbnails run flush with the toolbar.
            Layout.rightMargin: -root.gap
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.model
            readonly property int columns: Math.max(1, Math.floor(width / (root.minTileWidth + root.gap)))
            readonly property int tileWidth: Math.floor(width / columns) - root.gap
            readonly property int tileHeight: Math.round(tileWidth * 9 / 16)
            cellWidth: tileWidth + root.gap
            cellHeight: tileHeight + root.captionHeight + root.gap
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: cell
                required property int index
                required property string name
                required property string path
                required property string url
                required property var modified

                width: grid.cellWidth
                height: grid.cellHeight

                readonly property bool picked: root.isSelected(cell.path)

                HoverHandler { id: hover }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: tileMenu.popup()
                }

                Rectangle {
                    id: frame
                    width: grid.tileWidth
                    height: grid.tileHeight
                    radius: Theme.radius.lg
                    color: Theme.palette.surfaceSunken
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: "image://screenshot/" + encodeURIComponent(cell.path)
                        sourceSize: Qt.size(512, 512)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }

                    // Dark regardless of theme, so the actions stay readable over any screenshot.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.42
                        opacity: hover.hovered ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
                        }

                        RowLayout {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Theme.space.xs
                            spacing: Theme.space.xxs

                            IconButton {
                                size: Theme.control.heightSm
                                flat: false
                                iconName: "copy"
                                tip: qsTr("Copy image")
                                onClicked: {
                                    if (root.model.copyImage(cell.index))
                                        toast.show(qsTr("Image copied."), "success")
                                    else
                                        toast.show(qsTr("The image could not be copied."), "danger")
                                }
                            }
                            IconButton {
                                id: moreButton
                                size: Theme.control.heightSm
                                flat: false
                                iconName: "more"
                                tip: qsTr("More")
                                onClicked: tileMenu.popup(moreButton, moreButton.width - tileMenu.width, moreButton.height)
                            }
                        }
                    }

                    TapHandler {
                        onTapped: {
                            if (root.selectedCount > 0)
                                root.toggleSelected(cell.path)
                            else
                                Qt.openUrlExternally(cell.url)
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: Theme.space.xs
                        width: Theme.control.heightSm
                        height: Theme.control.heightSm
                        radius: Theme.radius.sm
                        color: Theme.media.chip
                        border.width: 1
                        border.color: Theme.media.chipBorder
                        visible: hover.hovered || root.selectedCount > 0

                        CheckBox {
                            anchors.centerIn: parent
                            checked: cell.picked
                            onToggled: {
                                root.toggleSelected(cell.path)
                                // toggled() replaced the binding with the user's value; put it back.
                                checked = Qt.binding(() => cell.picked)
                            }
                            Accessible.name: qsTr("Select %1").arg(cell.name)
                        }
                    }

                    CornerMatte {
                        cornerRadius: frame.radius
                        outline: cell.picked ? Theme.palette.accent
                                             : (hover.hovered ? Theme.palette.borderStrong : Theme.palette.border)
                    }
                }

                Menu {
                    id: tileMenu
                    MenuItem {
                        text: qsTr("Open")
                        icon.source: Icons.url("external-link")
                        onTriggered: Qt.openUrlExternally(cell.url)
                    }
                    MenuItem {
                        text: qsTr("Copy image")
                        icon.source: Icons.url("copy")
                        onTriggered: {
                            if (root.model.copyImage(cell.index))
                                toast.show(qsTr("Image copied."), "success")
                            else
                                toast.show(qsTr("The image could not be copied."), "danger")
                        }
                    }
                    MenuItem {
                        text: qsTr("Rename…")
                        icon.source: Icons.url("edit")
                        onTriggered: {
                            renamePrompt.path = cell.path
                            renamePrompt.value = root.baseName(cell.name)
                            renamePrompt.error = ""
                            renamePrompt.open()
                        }
                    }
                    MenuItem {
                        visible: root.canUpload
                        height: visible ? implicitHeight : 0
                        enabled: !!root.uploader && !root.uploader.busy
                        text: root.selectedCount > 1 && cell.picked ? qsTr("Upload selected to Imgur")
                                                                    : qsTr("Upload to Imgur")
                        icon.source: Icons.url("arrow-up")
                        onTriggered: root.upload(root.targets(cell.path))
                    }
                    MenuSeparator {}
                    MenuItem {
                        text: root.selectedCount > 1 && cell.picked ? qsTr("Delete selected…") : qsTr("Delete")
                        icon.source: Icons.url("trash")
                        onTriggered: {
                            var paths = root.targets(cell.path)
                            if (paths.length > 1) {
                                deleteConfirm.paths = paths
                                deleteConfirm.text = qsTr("Move %1 screenshots to the trash?").arg(paths.length)
                                deleteConfirm.open()
                            } else {
                                root.removeAll(paths)
                            }
                        }
                    }
                }

                Text {
                    anchors.top: frame.bottom
                    anchors.topMargin: Theme.space.xs
                    x: frame.x
                    width: frame.width
                    text: root.captionOf(cell.name)
                    elide: Text.ElideMiddle
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                    font.weight: Font.Medium
                }
            }
        }
    }

    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: grid
    }

    ConfirmDialog {
        id: deleteConfirm
        property var paths: []
        title: qsTr("Delete screenshots")
        confirmText: qsTr("Move to trash")
        onConfirmed: root.removeAll(paths)
    }

    PromptDialog {
        id: renamePrompt
        property string path
        title: qsTr("Rename screenshot")
        confirmText: qsTr("Rename")
        onSubmitted: (text) => {
            var row = root.rowOf(path)
            if (row >= 0 && root.model.rename(row, text)) {
                root.clearSelection()
                close()
            } else {
                error = qsTr("That name cannot be used, or another screenshot already has it.")
            }
        }
    }

    ClipboardText { id: clipboard }

    Toast { id: toast }

    EmptyState {
        anchors.centerIn: parent
        upperThird: true
        visible: !root.model || root.model.count === 0
        title: qsTr("No screenshots yet")
        body: qsTr("Press F2 in game; they will show up here.")
        MeshIcon { iconName: "image"; size: 40; color: Theme.palette.textTertiary }
    }
}
