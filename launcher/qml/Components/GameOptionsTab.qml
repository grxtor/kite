// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * This instance's options.txt - the widget-free replacement for
 * GameOptionsPage, which only ever showed it.
 *
 * The options a player actually recognises come first, grouped the way the
 * game's own Options screen groups them, in the game's own terms ("Render
 * distance: 12 chunks", not "renderDistance 12") and with the same kind of
 * control (GameOptionRow). A change is written to options.txt at once and
 * applies the next time the game starts; while it is running everything is
 * read-only, since the game rewrites the file as it quits. Every raw key is
 * still there, searchable, behind "All options".
 *
 * A TabScroll, like the Settings tab beside it: no heading (the tab strip
 * names the tab), one line of description, the page's full width.
 */
TabScroll {
    id: root

    // KeyValueFilterModel over the instance's GameOptions (InstanceDetails.gameOptions).
    property var model: null
    // Whether this instance's game is running (InstancePage.running).
    property bool running: false
    property bool showAll: false

    readonly property var store: root.model ? root.model.sourceModel : null
    readonly property bool editable: !root.running && collector.count > 0

    description: collector.count === 0
                 ? qsTr("This instance has no options yet. Play it once and the game creates them.")
                 : root.running
                 ? qsTr("The game is running. These can be changed again once it closes.")
                 : qsTr("What this instance's game is set to. Changes are saved straight away and apply the next time it starts.")

    // key -> raw value, read from the unfiltered GameOptions underneath the
    // search proxy, so the groups above never change while searching.
    property var values: ({})
    // The keys there are, one per line. A string only notifies when it
    // actually changes, so the rows below are rebuilt when a key appears or
    // goes -- never while a value is being edited.
    property string keyList: ""
    function collect() {
        const map = {}
        for (let i = 0; i < collector.count; ++i) {
            const o = collector.objectAt(i)
            if (o)
                map[o.key] = o.value
        }
        root.values = map
        root.keyList = Object.keys(map).join("\n")
    }
    Instantiator {
        id: collector
        model: root.store
        delegate: QtObject {
            required property string key
            required property string value
            onValueChanged: Qt.callLater(root.collect)
        }
        onObjectAdded: Qt.callLater(root.collect)
        onObjectRemoved: Qt.callLater(root.collect)
    }

    function has(key) { return ("\n" + root.keyList + "\n").indexOf("\n" + key + "\n") >= 0 }
    // options.txt writes its decimals the way the game does: "1.0", not "1".
    function decimal(v) { return Number.isInteger(v) ? v.toFixed(1) : String(v) }
    // "en_us" -> "English (United States)", in that language's own words.
    function language() {
        const parts = String(root.values.lang).split("_")
        const locale = Qt.locale(parts[0] + (parts.length > 1 ? "_" + parts[1].toUpperCase() : ""))
        if (locale.name === "C")
            return root.values.lang
        return parts.length > 1 ? qsTr("%1 (%2)").arg(locale.nativeLanguageName).arg(locale.nativeTerritoryName)
                                : locale.nativeLanguageName
    }
    // ["vanilla","fabric","file/Fresh Animations.zip"] -> "Fresh Animations"
    function resourcePacks() {
        let list = []
        try { list = JSON.parse(root.values.resourcePacks) } catch (e) { return root.values.resourcePacks }
        const named = list.filter((p) => p !== "vanilla" && p !== "fabric")
                          .map((p) => String(p).replace(/^file\//, "").replace(/\.zip$/i, ""))
        return named.length > 0 ? named.join(", ") : qsTr("None")
    }

    // Row specs for GameOptionRow (its header lists the fields).
    function chunks(label, key, from) {
        return { label: label, key: key, kind: "slider", from: from, to: 32, step: 1,
                 toSlider: (raw) => Number(raw), toRaw: (v) => String(v),
                 text: (v) => qsTr("%1 chunks").arg(v) }
    }
    // A 0..1 value shown as 0..scale.
    function fraction(label, key, scale, text) {
        return { label: label, key: key, kind: "slider", from: 0, to: scale, step: 1,
                 toSlider: (raw) => Math.round(Number(raw) * scale),
                 toRaw: (v) => root.decimal(v / scale), text: text }
    }
    function volume(label, key) {
        return root.fraction(label, key, 100, (v) => v === 0 ? qsTr("Off") : qsTr("%1%").arg(v))
    }
    function toggle(label, key) { return { label: label, key: key, kind: "switch" } }
    function info(label, key, text) { return { label: label, key: key, kind: "text", text: text } }

    // The game's own stops: options.txt stores the field of view as
    // (fov - 70) / 40, and 260 is the framerate slider's "Unlimited".
    readonly property int normalFov: 70
    readonly property int fovSpan: 40
    readonly property int unlimitedFps: 260
    readonly property int maxGuiScale: 6

    // { title, rows: [spec] } -- a row only shows when its key is in this
    // options.txt (fancyGraphics is what versions before 1.16 wrote instead
    // of graphicsMode).
    // (Not "groups": that is TabScroll's default property.)
    readonly property var optionGroups: [
        { title: qsTr("Video"), rows: [
            root.chunks(qsTr("Render distance"), "renderDistance", 2),
            root.chunks(qsTr("Simulation distance"), "simulationDistance", 5),
            { label: qsTr("Field of view"), key: "fov", kind: "slider",
              from: root.normalFov - root.fovSpan, to: root.normalFov + root.fovSpan, step: 1,
              toSlider: (raw) => Math.round(root.normalFov + Number(raw) * root.fovSpan),
              toRaw: (deg) => root.decimal((deg - root.normalFov) / root.fovSpan),
              text: (deg) => deg === root.normalFov ? qsTr("Normal")
                           : deg === root.normalFov + root.fovSpan ? qsTr("Quake Pro") : String(deg) },
            { label: qsTr("Max framerate"), key: "maxFps", kind: "slider", from: 10, to: root.unlimitedFps, step: 10,
              toSlider: (raw) => Number(raw), toRaw: (v) => String(v),
              text: (v) => v >= root.unlimitedFps ? qsTr("Unlimited") : qsTr("%1 fps").arg(v) },
            { label: qsTr("Graphics"), key: "graphicsMode", kind: "choice", choices: [
                { value: "0", label: qsTr("Fast") }, { value: "1", label: qsTr("Fancy") },
                { value: "2", label: qsTr("Fabulous!") }] },
            { label: qsTr("Graphics"), key: "fancyGraphics", kind: "choice", choices: [
                { value: "false", label: qsTr("Fast") }, { value: "true", label: qsTr("Fancy") }] },
            root.fraction(qsTr("Brightness"), "gamma", 100,
                          (v) => v === 0 ? qsTr("Moody") : v === 100 ? qsTr("Bright") : qsTr("%1%").arg(v)),
            { label: qsTr("GUI scale"), key: "guiScale", kind: "choice",
              choices: [{ value: "0", label: qsTr("Auto") }].concat(
                  Array.from({ length: root.maxGuiScale }, (_, i) => ({ value: String(i + 1), label: String(i + 1) }))) },
            root.toggle(qsTr("VSync"), "enableVsync"),
            root.toggle(qsTr("Fullscreen"), "fullscreen")
        ] },
        { title: qsTr("Music and sounds"), rows: [
            root.volume(qsTr("Master volume"), "soundCategory_master"),
            root.volume(qsTr("Music"), "soundCategory_music"),
            root.volume(qsTr("Jukebox and note blocks"), "soundCategory_record"),
            root.volume(qsTr("Weather"), "soundCategory_weather"),
            root.volume(qsTr("Blocks"), "soundCategory_block"),
            root.volume(qsTr("Hostile creatures"), "soundCategory_hostile"),
            root.volume(qsTr("Friendly creatures"), "soundCategory_neutral"),
            root.volume(qsTr("Players"), "soundCategory_player"),
            root.volume(qsTr("Ambient"), "soundCategory_ambient"),
            root.volume(qsTr("Voice"), "soundCategory_voice")
        ] },
        { title: qsTr("Controls"), rows: [
            // 0.5 is the game's 100%.
            root.fraction(qsTr("Mouse sensitivity"), "mouseSensitivity", 200, (v) => qsTr("%1%").arg(v)),
            root.toggle(qsTr("Auto-jump"), "autoJump")
        ] },
        // Read-only: the language list is the game's own, and packs are
        // turned on and off on the Content tab.
        { title: qsTr("Language and packs"), rows: [
            root.info(qsTr("Language"), "lang", root.language),
            root.info(qsTr("Resource packs"), "resourcePacks", root.resourcePacks)
        ] }
    ]

    Repeater {
        model: root.optionGroups
        delegate: SettingsGroup {
            id: group
            required property var modelData
            readonly property var shownRows: modelData.rows.filter((r) => root.has(r.key))
            width: parent ? parent.width : 0
            visible: shownRows.length > 0
            title: modelData.title

            Repeater {
                model: group.shownRows
                delegate: GameOptionRow {
                    required property var modelData
                    spec: modelData
                    raw: root.values[modelData.key] !== undefined ? root.values[modelData.key] : ""
                    store: root.store
                    editable: root.editable
                }
            }
        }
    }

    // Every key, raw, for anything the groups above leave out.
    Column {
        width: parent.width
        spacing: Theme.space.sm
        visible: collector.count > 0

        Button {
            flat: true
            leftPadding: 0
            text: root.showAll ? qsTr("Hide all options") : qsTr("All options (%1)").arg(collector.count)
            icon.source: Icons.url(root.showAll ? "chevron-down" : "chevron-right")
            onClicked: root.showAll = !root.showAll
        }

        SearchBox {
            visible: root.showAll
            width: parent.width
            placeholderText: qsTr("Search options")
            onTextChanged: if (root.model) root.model.filterText = text
        }

        Rectangle {
            visible: root.showAll
            width: parent.width
            height: rawColumn.height
            radius: Theme.radius.lg
            color: Theme.palette.surface
            border.width: 1
            border.color: Theme.palette.border

            Column {
                id: rawColumn
                width: parent.width
                topPadding: Theme.space.xs
                bottomPadding: Theme.space.xs

                Repeater {
                    id: rawRows
                    model: root.showAll ? root.model : null
                    delegate: RowLayout {
                        required property string key
                        required property string value
                        x: Theme.space.lg
                        width: rawColumn.width - Theme.space.lg * 2
                        height: Theme.control.height
                        spacing: Theme.space.md

                        Text {
                            Layout.preferredWidth: parent.width / 2
                            text: key
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.mono
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                        Text {
                            Layout.fillWidth: true
                            text: value
                            elide: Text.ElideRight
                            color: Theme.palette.textSecondary
                            font.family: Theme.font.mono
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                    }
                }

                Text {
                    visible: rawRows.count === 0
                    x: Theme.space.lg
                    width: rawColumn.width - Theme.space.lg * 2
                    height: Theme.control.height
                    verticalAlignment: Text.AlignVCenter
                    text: root.model ? qsTr("Nothing matches “%1”.").arg(root.model.filterText) : ""
                    elide: Text.ElideRight
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
            }
        }
    }
}
