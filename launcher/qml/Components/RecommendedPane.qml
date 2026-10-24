// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The New instance dialog's Recommended tab: a preset (Kite Performance,
 * Kite PvP, Kite PvP 1.8.9) from the catalog, a Minecraft version for the
 * presets that take any, and the preset's mods as a checklist. Under it, a
 * live line says what will be installed for that version: the picked mods
 * are looked up on Modrinth a moment after the last change
 * (BundleCatalog::preview()), and the mods with no compatible version are
 * named, since they are skipped and the rest still installs.
 *
 * The pane owns the preview. The dialog asks takeReview() for it when
 * Create is pressed and hands it to BundleCatalog::installInto() once the
 * instance exists; anything not taken is cancelled.
 */
ColumnLayout {
    id: root

    // NewInstanceController, and BundleCatalog (shell.bundles).
    property var controller: null
    property var bundles: null
    // The pane is the dialog's current tab. It borrows the controller's
    // loader and version while it is, and gives the loader back after.
    property bool active: false
    property bool busy: false
    // Loader id -> display name, from the dialog.
    property var loaderLabel: (value) => value

    readonly property var presets: root.bundles ? root.bundles.presets : []
    property int presetIndex: 0
    readonly property var preset: root.presets.length > 0 ? root.presets[Math.min(root.presetIndex, root.presets.length - 1)] : null
    readonly property bool fixedVersion: !!root.preset && root.preset.minecraft.length > 0

    // Release versions, newest first, collected from the controller's model.
    property var releases: []
    property string pickedVersion: ""
    readonly property string version: !root.preset ? ""
        : root.fixedVersion ? root.preset.minecraft
        : root.releases.indexOf(root.pickedVersion) >= 0 ? root.pickedVersion
        : root.releases.length > 0 ? root.releases[0] : ""

    readonly property var mods: root.bundles && root.preset ? root.bundles.mods(root.preset.bundles, root.preset.loader) : []
    readonly property var checked: modList.checkedProjects

    // The review names a picked mod it found nothing for by its project id.
    readonly property var unresolvedNames: {
        if (!root.review)
            return []
        return root.review.unresolved.map((name) => {
            const mod = root.mods.find((m) => m.project === name)
            return mod ? mod.name : name
        })
    }
    // The ContentInstallReview of `checked` for `version`, or null.
    property var review: null
    readonly property bool lookingUp: previewTimer.running || (!!root.review && root.review.resolving)
    readonly property bool loaderMissing: !!root.controller && !root.controller.loaderLoading
                                          && root.controller.selectedLoaderVersion.length === 0
    readonly property bool canCreate: !root.busy && !!root.preset && root.version.length > 0
                                      && !root.loaderMissing && !previewTimer.running
                                      && (root.checked.length === 0 || (!!root.review && root.review.ready))
    // What the dialog's name field starts with.
    readonly property string suggestedName: !root.preset || root.version.length === 0 ? ""
        : root.preset.name.indexOf(root.version) >= 0 ? root.preset.name : root.preset.name + " " + root.version

    readonly property int previewDebounceMs: 400
    readonly property int cardMinHeight: Theme.control.heightLg + Theme.space.xl
    readonly property int modListMinHeight: 4 * Theme.control.heightLg
    readonly property int versionBoxWidth: 220
    readonly property string optiFineUrl: "https://optifine.net/downloads"

    spacing: Theme.space.lg

    // The pending review goes to the caller, which installs or cancels it.
    function takeReview() {
        const taken = root.review
        root.review = null
        return taken
    }
    function dropReview() {
        if (root.review)
            root.review.cancel()
        root.review = null
    }
    function schedulePreview() {
        root.dropReview()
        if (root.active)
            previewTimer.restart()
    }

    // The loader and version the controller makes the instance with.
    property string loaderBefore: ""
    function applyToController() {
        if (!root.active || !root.controller || !root.preset)
            return
        root.controller.loader = root.preset.loader
        if (root.version.length > 0)
            root.controller.selectMinecraftVersion(root.version)
    }
    function collectReleases() {
        const out = []
        for (let i = 0; i < collector.count; ++i) {
            const item = collector.objectAt(i)
            if (item && item.type === "release")
                out.push(item.version)
        }
        root.releases = out
    }

    onActiveChanged: {
        if (root.active) {
            root.loaderBefore = root.controller ? root.controller.loader : ""
            root.applyToController()
            root.schedulePreview()
        } else {
            root.dropReview()
            previewTimer.stop()
            if (root.controller)
                root.controller.loader = root.loaderBefore
        }
    }
    onPresetChanged: { root.applyToController(); root.schedulePreview() }
    onVersionChanged: { root.applyToController(); root.schedulePreview() }
    onCheckedChanged: root.schedulePreview()

    Instantiator {
        id: collector
        model: root.controller ? root.controller.minecraftVersions : null
        delegate: QtObject {
            required property string version
            required property string type
        }
        onObjectAdded: Qt.callLater(root.collectReleases)
        onObjectRemoved: Qt.callLater(root.collectReleases)
    }

    Timer {
        id: previewTimer
        interval: root.previewDebounceMs
        onTriggered: {
            if (root.active && root.bundles && root.preset && root.version.length > 0 && root.checked.length > 0)
                root.review = root.bundles.preview(root.checked, root.version, root.preset.loader)
        }
    }

    // Presets, in the loader cards' look.
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.sm

        Repeater {
            model: root.presets

            delegate: AbstractButton {
                id: card
                required property int index
                required property var modelData
                readonly property bool selected: root.presetIndex === index

                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.alignment: Qt.AlignTop
                Layout.preferredHeight: Math.max(root.cardMinHeight, cardColumn.implicitHeight + Theme.space.md * 2)
                enabled: !root.busy
                hoverEnabled: true
                checkable: true
                checked: selected
                Accessible.name: modelData.name
                onClicked: root.presetIndex = index

                background: Rectangle {
                    radius: Theme.radius.md
                    color: card.selected ? Theme.palette.accentSubtle
                         : card.hovered ? Theme.palette.hoverOverlay : "transparent"
                    border.width: 1
                    border.color: card.selected ? Theme.palette.accent
                                : card.hovered ? Theme.palette.borderStrong : Theme.palette.border
                    Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                    Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
                }

                contentItem: Column {
                    id: cardColumn
                    spacing: Theme.space.xxs
                    leftPadding: Theme.space.md
                    rightPadding: Theme.space.md
                    topPadding: Theme.space.md
                    Text {
                        width: parent.width - Theme.space.md * 2
                        text: card.modelData.name
                        elide: Text.ElideRight
                        color: card.selected ? Theme.palette.accentText : Theme.palette.textPrimary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.body.pixelSize
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width - Theme.space.md * 2
                        text: card.modelData.summary
                        wrapMode: Text.Wrap
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                    Text {
                        width: parent.width - Theme.space.md * 2
                        text: card.modelData.minecraft.length > 0
                              ? qsTr("%1 · Minecraft %2").arg(root.loaderLabel(card.modelData.loader)).arg(card.modelData.minecraft)
                              : qsTr("%1 · any recent version").arg(root.loaderLabel(card.modelData.loader))
                        elide: Text.ElideRight
                        color: Theme.palette.textTertiary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
            }
        }
    }

    // Minecraft version: the newest release, or the one a preset needs.
    RowLayout {
        Layout.fillWidth: true
        Layout.preferredHeight: Theme.control.height
        spacing: Theme.space.md

        Text {
            text: qsTr("Minecraft version")
            color: Theme.palette.textPrimary
            font.family: Theme.font.heading
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Font.Bold
        }
        Item { Layout.fillWidth: true }
        Text {
            visible: root.fixedVersion
            text: qsTr("Minecraft %1").arg(root.version)
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: Font.Medium
        }
        ComboBox {
            Layout.preferredWidth: root.versionBoxWidth
            visible: !root.fixedVersion
            enabled: !root.busy && count > 0
            model: root.releases
            currentIndex: root.releases.indexOf(root.version)
            displayText: count === 0 ? qsTr("Loading…") : currentText
            onActivated: root.pickedVersion = currentText
        }
    }

    BundleModList {
        id: modList
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: root.modListMinHeight
        Layout.preferredHeight: root.modListMinHeight
        enabled: !root.busy
        mods: root.mods
    }

    // OptiFine is not on Modrinth and its license forbids bundling it.
    RowLayout {
        Layout.fillWidth: true
        visible: !!root.preset && root.preset.id === "pvp-legacy"
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: qsTr("Kite can't download OptiFine, because its license doesn't allow it. Get it from optifine.net and drop it on the instance's Mods tab.")
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
        Button {
            text: qsTr("Open optifine.net")
            icon.source: Icons.url("external-link")
            onClicked: Qt.openUrlExternally(root.optiFineUrl)
        }
    }

    // What the pick comes to.
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space.xxs

        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            text: root.loaderMissing ? qsTr("%1 has no build for Minecraft %2.").arg(root.preset ? root.loaderLabel(root.preset.loader) : "").arg(root.version)
                : root.version.length === 0 ? qsTr("Pick a version to see what will be installed.")
                : root.checked.length === 0 ? qsTr("No mods picked. Only the loader will be installed.")
                : root.lookingUp || !root.review ? qsTr("Looking up versions…")
                : qsTr("%n mods will be installed, with what they need.", "", root.review.count)
        }
        Text {
            Layout.fillWidth: true
            visible: !!root.review && root.review.ready && root.review.unresolved.length > 0
            wrapMode: Text.Wrap
            color: Theme.palette.warning
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.Medium
            text: root.review ? qsTr("Not available for %1: %2. They are skipped.").arg(root.version).arg(root.unresolvedNames.join(", ")) : ""
        }
    }
}
