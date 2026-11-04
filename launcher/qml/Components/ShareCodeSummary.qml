// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * What a share code carries, in two quiet lines under its name: "Minecraft 1.21.4 · Fabric 0.16.10" and
 * "12 mods · 2 resource packs · 1 shader pack" (or "No mods or packs"). `info` is a ShareCodes::toMap() or
 * ShareManifest::toMap() map. The name is plain text: it is whatever the person who shared it typed.
 */
Column {
    id: root

    property var info: ({})
    // Off where a title above already names it.
    property bool showName: true

    readonly property var loaderNames: ({ fabric: "Fabric", quilt: "Quilt", forge: "Forge", neoforge: "NeoForge" })

    function versionText(info) {
        var parts = [qsTr("Minecraft %1").arg(info.minecraftVersion || "")]
        var loader = root.loaderNames[info.loader]
        if (loader)
            parts.push(loader + " " + (info.loaderVersion || ""))
        return parts.join(" · ")
    }
    // The singular or the plural source string by count, so an untranslated build never prints "mod(s)".
    function contentText(info) {
        var parts = []
        var mods = info.mods || 0
        var resourcePacks = info.resourcePacks || 0
        var shaderPacks = info.shaderPacks || 0
        var dataPacks = info.dataPacks || 0
        if (mods > 0)
            parts.push(mods === 1 ? qsTr("1 mod") : qsTr("%1 mods").arg(mods))
        if (resourcePacks > 0)
            parts.push(resourcePacks === 1 ? qsTr("1 resource pack") : qsTr("%1 resource packs").arg(resourcePacks))
        if (shaderPacks > 0)
            parts.push(shaderPacks === 1 ? qsTr("1 shader pack") : qsTr("%1 shader packs").arg(shaderPacks))
        if (dataPacks > 0)
            parts.push(dataPacks === 1 ? qsTr("1 data pack") : qsTr("%1 data packs").arg(dataPacks))
        return parts.length > 0 ? parts.join(" · ") : qsTr("No mods or packs")
    }

    spacing: Theme.space.xxs

    Copy {
        width: root.width
        visible: root.showName
        text: root.info.name || ""
        textFormat: Text.PlainText
        role: "bodyStrong"
        elide: Text.ElideRight
    }
    Copy {
        width: root.width
        text: root.versionText(root.info)
        role: "label"
        tone: "secondary"
        elide: Text.ElideRight
    }
    Copy {
        width: root.width
        text: root.contentText(root.info)
        role: "label"
        tone: "secondary"
        wrapMode: Text.Wrap
    }
}
