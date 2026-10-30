// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton
import QtQuick
import Kite.Theme

/*
 * Icon source lookup for code that wants a plain url rather than a MeshIcon
 * instance -- e.g. an IconLabel/Button icon.source binding. The qrc path is
 * spelled out in full (rather than Qt.resolvedUrl, which MeshIcon uses) since
 * a singleton has no per-instance "own file" to resolve relative to.
 */
QtObject {
    function url(name) {
        return "qrc:/qt/qml/Kite/Components/icons/" + name + ".svg"
    }

    // The image://instanceicon URL for an icon key (empty = the generic
    // icon). Every instance icon goes through here: QML caches an image by
    // its URL, so the "?rev=" is what makes a changed game texture or
    // replaced icon file load again (see InstanceIconProvider.h).
    // Whether instance icon `key` is an opaque square to its edges (a block,
    // a mob's face, most pack logos; IconList::fillsTile()). Such an icon
    // fills its tile, with the tile's own corners (KiteShape's matte), as a
    // pack logo does; any other one sits inside it.
    function fillsTile(key) {
        LauncherStore.iconRevision // a changed icon asks again
        return !!LauncherStore.icons
               && LauncherStore.icons.iconFillsTile(key && key.length > 0 ? key : "default")
    }

    function instanceIcon(key) {
        return "image://instanceicon/" + (key && key.length > 0 ? key : "default")
               + "?rev=" + LauncherStore.iconRevision
    }

    // Whether instance icon `key` is one nobody picked: the generic one every
    // new instance starts with, or the bare platform logo an import sets
    // when the pack has none of its own (AutoIcons::isGeneric).
    function isGeneric(key) {
        return !key || key.length === 0 || key === "default" || key === "flame" || key === "modrinth"
    }

    // The icon key to draw for an instance: its own, or -- in the Sahne,
    // Sade and Gece looks, while its own is the generic one -- the built-in
    // icon the launcher gave it (QmlShell.autoIcons, one per instance while
    // the set lasts), so a library of new instances is not one grass block
    // after another. Klasik keeps the grass block.
    function displayKey(key, instanceId) {
        if (Theme.look === "classic" || !isGeneric(key) || !LauncherStore.icons)
            return key
        var given = LauncherStore.icons.autoIcons
        return given && given[instanceId] ? given[instanceId] : key
    }
}
