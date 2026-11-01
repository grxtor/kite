// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls.impl
import Kite.Theme

/*
 * A themed line icon: wraps IconImage so callers pick an icon by name from
 * icons/. The SVGs are pure #000000 on transparent, so IconImage's alpha-channel
 * recolouring can tint them to any palette colour.
 */
IconImage {
    // Not `name`: IconImage already has a FINAL `name` (a theme icon name).
    property string iconName
    property int size: Theme.icon.md

    width: size
    height: size
    sourceSize: Qt.size(size, size)
    source: iconName.length > 0 ? Qt.resolvedUrl("icons/" + iconName + ".svg") : ""
    color: Theme.palette.textSecondary
}
