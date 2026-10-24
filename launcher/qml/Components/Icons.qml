// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

pragma Singleton
import QtQuick

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
}
