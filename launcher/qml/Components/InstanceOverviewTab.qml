// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The instance at a glance: its recent screenshots and worlds, what is
 * installed, and the user's own notes -- each a short summary that hands
 * off to the tab with the full picture. Nothing here repeats the banner
 * above it (loader, version, play time, last played already live there).
 *
 * Not a SettingsScroll: that caps its column at 760px for reading comfort,
 * which is right for a form but starves this two-column board and its
 * screenshot strip of the width they need.
 */
Item {
    id: root

    // InstanceDetails, for the screenshots/worlds/mods previews below.
    property var details: null
    property string notes
    signal notesEdited(string text)
    signal screenshotsRequested()
    signal worldsRequested()
    signal manageContentRequested()

    readonly property var shotsModel: root.details ? root.details.screenshots : null
    readonly property var worldsModel: root.details ? root.details.worlds : null
    readonly property var modsModel: root.details ? root.details.mods : null
    readonly property bool isMinecraft: !!root.details && root.details.isMinecraft
    // An old-format instance: it can only be upgraded, not played or edited.
    readonly property bool isLegacy: !!root.details && root.details.isLegacy
    // The upgrade in progress (a TaskWatcher), if one was started here.
    property var upgrade: null
    readonly property bool upgrading: !!root.upgrade && root.upgrade.running
    onDetailsChanged: root.upgrade = null

    // How much of each list the board previews before handing off.
    readonly property int shotsShown: 4
    readonly property int worldsShown: 3
    readonly property int modsShown: 6
    // Two columns once the side column leaves the main one room to breathe.
    readonly property int twoColumnWidth: 760
    readonly property int sideColumnWidth: 300
    readonly property int notesHeight: 120
    readonly property int worldThumbSize: 40
    // Each mod's logo tile in the Mods card.
    readonly property int modIconSize: 24

    Accessible.name: qsTr("Overview")

    // Worlds most recently played first. WorldList keeps folder order, so
    // the rows are collected here and sorted; `revision` re-sorts whenever
    // one of them changes.
    property int worldsRevision: 0
    readonly property var recentWorlds: {
        void root.worldsRevision
        const rows = []
        for (let i = 0; i < worldsCollector.count; ++i) {
            const w = worldsCollector.objectAt(i)
            if (w)
                rows.push(w)
        }
        rows.sort((a, b) => Number(b.lastPlayed || 0) - Number(a.lastPlayed || 0))
        return rows.slice(0, root.worldsShown)
    }
    Instantiator {
        id: worldsCollector
        model: root.worldsModel
        delegate: QtObject {
            required property int index
            required property string name
            required property string folder
            required property var lastPlayed
            required property var iconFile
            onLastPlayedChanged: root.worldsRevision++
            onNameChanged: root.worldsRevision++
        }
        onObjectAdded: root.worldsRevision++
        onObjectRemoved: root.worldsRevision++
    }

    // Shared look of the board's cards.
    component Card: Rectangle {
        id: card
        default property alias body: cardColumn.data
        property string title
        property string actionText
        signal actionTriggered()

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border
        implicitHeight: cardColumn.implicitHeight + Theme.space.lg * 2

        ColumnLayout {
            id: cardColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.space.lg
            spacing: Theme.space.md

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.control.height
                spacing: Theme.space.sm
                Text {
                    Layout.fillWidth: true
                    text: card.title
                    elide: Text.ElideRight
                    color: Theme.palette.textPrimary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.title.pixelSize
                    font.weight: Theme.type.title.weight
                }
                LinkButton {
                    visible: card.actionText.length > 0
                    text: card.actionText
                    onClicked: card.actionTriggered()
                }
            }
        }
    }

    component Hint: Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.palette.textTertiary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.label.pixelSize
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + Theme.space.xl
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        // Overlays the content's right edge rather than reserving a gutter.
        ScrollBar.vertical: ScrollBar {}

        GridLayout {
            id: content
            width: flick.width
            columns: width >= root.twoColumnWidth ? 2 : 1
            columnSpacing: Theme.space.lg
            rowSpacing: Theme.space.lg

            // Main column: what the user made in this instance.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space.lg

                // Old-format instance: the one thing to do with it. Mirrors the
                // widget's "Upgrade is required" page.
                Card {
                    visible: root.isLegacy
                    title: qsTr("Upgrade is required")

                    Hint {
                        text: qsTr("MeshMC now runs old Minecraft versions in its newer instance format, and the old (legacy) format is switched off, so this instance has to be upgraded before it can be played.")
                    }
                    Hint {
                        text: qsTr("The upgrade makes a copy of this instance in the new format. The original stays as it is, in case anything goes wrong.")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.md

                        Button {
                            highlighted: true
                            enabled: !root.upgrading && !!root.details && root.details.contentChangesAllowed
                            text: root.upgrading ? qsTr("Upgrading…") : qsTr("Upgrade to a new instance")
                            onClicked: root.upgrade = root.details.upgradeLegacy()
                        }
                        LaunchProgressBar {
                            Layout.fillWidth: true
                            visible: root.upgrading
                            progress: root.upgrade ? root.upgrade.progress : -1
                        }
                    }

                    Hint {
                        visible: !!root.details && !root.details.contentChangesAllowed
                        text: qsTr("Close the game first.")
                    }
                    Hint {
                        visible: !!root.upgrade && root.upgrade.failed
                        text: root.upgrade ? (root.upgrade.error || qsTr("The upgrade did not work.")) : ""
                        color: Theme.palette.danger
                    }
                    Hint {
                        visible: !!root.upgrade && root.upgrade.succeeded
                        text: qsTr("Done. The upgraded instance is in your library.")
                        color: Theme.palette.success
                    }
                }

                Card {
                    title: qsTr("Recent screenshots")
                    actionText: !!root.shotsModel && root.shotsModel.count > 0 ? qsTr("View all") : ""
                    onActionTriggered: root.screenshotsRequested()

                    // A plain Item sized by the layout, with the tiles in a Row
                    // inside it: the tiles are sized from this Item's width,
                    // and an Item's implicit width stays 0, so they never feed
                    // back into the layout that sets it (a Row placed in the
                    // layout directly did, and looped polish()).
                    Item {
                        id: shotsRow
                        Layout.fillWidth: true
                        implicitHeight: tileHeight
                        visible: !!root.shotsModel && root.shotsModel.count > 0

                        readonly property int gap: Theme.space.sm
                        readonly property int tileWidth: Math.max(0, Math.floor((width - gap * (root.shotsShown - 1)) / root.shotsShown))
                        readonly property int tileHeight: Math.round(tileWidth * 9 / 16)

                        Row {
                            spacing: shotsRow.gap

                            Repeater {
                                model: root.shotsModel
                                delegate: Rectangle {
                                    id: shot
                                    required property int index
                                    required property string path

                                    visible: index < root.shotsShown
                                    width: shotsRow.tileWidth
                                    height: shotsRow.tileHeight
                                    radius: Theme.radius.sm
                                    color: Theme.palette.surfaceSunken
                                    clip: true

                                    HoverHandler { id: shotHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: root.screenshotsRequested() }

                                    Image {
                                        anchors.fill: parent
                                        source: "image://screenshot/" + encodeURIComponent(shot.path)
                                        sourceSize: Qt.size(320, 320)
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                    }
                                    CornerMatte {
                                        cornerRadius: shot.radius
                                        matte: Theme.palette.surface
                                        outline: shotHover.hovered ? Theme.palette.borderStrong : Theme.palette.border
                                    }
                                }
                            }
                        }
                    }

                    Hint {
                        visible: !root.shotsModel || root.shotsModel.count === 0
                        text: qsTr("Press F2 in game and your screenshots show up here.")
                    }
                }

                Card {
                    visible: root.isMinecraft
                    title: qsTr("Worlds")
                    actionText: root.recentWorlds.length > 0 ? qsTr("View all") : ""
                    onActionTriggered: root.worldsRequested()

                    Repeater {
                        model: root.recentWorlds
                        delegate: RowLayout {
                            id: worldRow
                            required property var modelData
                            readonly property string worldName: modelData.name.length > 0 ? modelData.name
                                                                                           : worldRow.folderName
                            // The folder's own name, the seed Home's world tiles
                            // use too, so a world without an icon shows the same
                            // scene on both pages.
                            readonly property string folderName: String(modelData.folder).split(/[\\/]/).pop()
                            readonly property string iconUrl: Format.fileUrl(modelData.iconFile)

                            Layout.fillWidth: true
                            spacing: Theme.space.md

                            Rectangle {
                                id: worldThumb
                                Layout.preferredWidth: root.worldThumbSize
                                Layout.preferredHeight: root.worldThumbSize
                                radius: Theme.radius.sm
                                color: Theme.palette.surfaceSunken
                                clip: true

                                Image {
                                    id: worldIcon
                                    anchors.fill: parent
                                    source: worldRow.iconUrl
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: status === Image.Ready
                                }
                                Image {
                                    anchors.fill: parent
                                    visible: !worldIcon.visible
                                    source: worldIcon.visible ? "" : PixelArt.worldUrlFor(worldRow.folderName)
                                    sourceSize: Qt.size(width * 2, height * 2)
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                    mipmap: true
                                    asynchronous: true
                                }
                                CornerMatte {
                                    cornerRadius: worldThumb.radius
                                    matte: Theme.palette.surface
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: Theme.space.xxs
                                Text {
                                    width: parent.width
                                    text: worldRow.worldName
                                    elide: Text.ElideRight
                                    color: Theme.palette.textPrimary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                    font.weight: Theme.type.label.weight
                                }
                                Text {
                                    width: parent.width
                                    text: Format.lastPlayed(worldRow.modelData.lastPlayed ? Number(worldRow.modelData.lastPlayed) : 0)
                                    elide: Text.ElideRight
                                    color: Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.caption.pixelSize
                                }
                            }

                            TapHandler { onTapped: root.worldsRequested() }
                        }
                    }

                    Hint {
                        visible: root.recentWorlds.length === 0
                        text: qsTr("Worlds you create in single player show up here.")
                    }
                }
            }

            // Side column: what is installed, and the user's notes.
            ColumnLayout {
                Layout.fillWidth: content.columns === 1
                Layout.preferredWidth: content.columns === 2 ? root.sideColumnWidth : -1
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space.lg

                Card {
                    title: modsRepeater.count > 0 ? qsTr("Mods (%1)").arg(modsRepeater.count) : qsTr("Mods")
                    actionText: modsRepeater.count > 0 ? qsTr("Manage") : ""
                    onActionTriggered: root.manageContentRequested()

                    Column {
                        Layout.fillWidth: true
                        spacing: Theme.space.sm
                        visible: modsRepeater.count > 0

                        Repeater {
                            id: modsRepeater
                            model: root.modsModel
                            delegate: Row {
                                id: modRow
                                required property int index
                                // Through `model`: a delegate property called
                                // "enabled" would disable the row itself.
                                required property var model
                                visible: index < root.modsShown
                                width: parent.width
                                spacing: Theme.space.sm

                                // The mod's own logo when the jar declares one
                                // (ModFolderModel's iconUrl), a neutral glyph
                                // otherwise; faded with the name when the mod
                                // is turned off, so on/off still reads at a
                                // glance without a coloured dot.
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: root.modIconSize
                                    height: root.modIconSize
                                    radius: Theme.radius.sm
                                    color: Theme.palette.surfaceRaised
                                    opacity: modRow.model.enabled ? 1.0 : Theme.opacity.disabled

                                    Image {
                                        id: modLogo
                                        anchors.fill: parent
                                        visible: status === Image.Ready
                                        source: modRow.model.iconUrl || ""
                                        sourceSize: Qt.size(root.modIconSize * 2, root.modIconSize * 2)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        mipmap: true
                                        asynchronous: true
                                    }
                                    MeshIcon {
                                        anchors.centerIn: parent
                                        visible: !modLogo.visible
                                        iconName: "cube"
                                        size: Theme.icon.sm
                                        color: Theme.palette.textTertiary
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - root.modIconSize - parent.spacing
                                    text: modRow.model.name
                                    elide: Text.ElideRight
                                    color: modRow.model.enabled ? Theme.palette.textPrimary : Theme.palette.textTertiary
                                    font.family: Theme.font.family
                                    font.pixelSize: Theme.type.label.pixelSize
                                }
                            }
                        }

                        Text {
                            visible: modsRepeater.count > root.modsShown
                            text: qsTr("and %1 more").arg(modsRepeater.count - root.modsShown)
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }

                    Hint {
                        visible: modsRepeater.count === 0
                        text: root.isMinecraft ? qsTr("No mods installed yet.")
                                               : qsTr("This instance cannot have mods.")
                    }
                }

                Card {
                    title: qsTr("Notes")

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.notesHeight
                        radius: Theme.radius.sm
                        color: Theme.palette.surfaceSunken
                        border.width: 1
                        border.color: notesArea.activeFocus ? Theme.palette.borderStrong : Theme.palette.border

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: Theme.space.xs

                            TextArea {
                                id: notesArea
                                placeholderText: qsTr("Seed, server address, which mods to update…")
                                wrapMode: TextEdit.Wrap
                                selectByMouse: true
                                background: null
                                color: Theme.palette.textPrimary
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                Component.onCompleted: text = root.notes
                                onActiveFocusChanged: if (!activeFocus && text !== root.notes) root.notesEdited(text)
                            }
                        }
                    }
                }
            }
        }
    }

    // Rows fade out at the fold instead of being sliced by the play bar.
    ScrollFade {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        flickable: flick
    }
}
