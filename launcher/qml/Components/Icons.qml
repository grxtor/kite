// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton
import QtQuick
import Kite.Theme

/*
 * Icon source lookup for code that wants a plain url rather than a MeshIcon
 * (e.g. an IconLabel/Button icon.source binding). A singleton has no own file
 * to resolve relative to, so the qrc path is spelled out.
 */
QtObject {
    function url(name) {
        return "qrc:/qt/qml/Kite/Components/icons/" + name + ".svg"
    }

    // Whether instance icon `key` is an opaque square to its edges (a block, a mob's face, most
    // pack logos; IconList::fillsTile()). Such an icon fills its tile; any other one sits inside it.
    function fillsTile(key) {
        LauncherStore.iconRevision // a changed icon asks again
        return !!LauncherStore.icons
               && LauncherStore.icons.iconFillsTile(key && key.length > 0 ? key : "default")
    }

    // Every instance icon goes through here: QML caches an image by its URL, so "?rev=" makes a
    // changed game texture or replaced icon file load again (see InstanceIconProvider.h).
    function instanceIcon(key) {
        return "image://instanceicon/" + (key && key.length > 0 ? key : "default")
               + "?rev=" + LauncherStore.iconRevision
    }

    // Whether `key` is one nobody picked: the generic icon new instances start with, or the bare
    // platform logo an import sets when the pack has none (AutoIcons::isGeneric).
    function isGeneric(key) {
        return !key || key.length === 0 || key === "default" || key === "flame" || key === "modrinth"
    }

    // The key to draw for an instance: its own, or in the Sahne, Sade and Gece looks, while its own
    // is generic, the icon the launcher gave it (QmlShell.autoIcons).
    function displayKey(key, instanceId) {
        if (Theme.look === "classic" || !isGeneric(key) || !LauncherStore.icons)
            return key
        var given = LauncherStore.icons.autoIcons
        return given && given[instanceId] ? given[instanceId] : key
    }
}
