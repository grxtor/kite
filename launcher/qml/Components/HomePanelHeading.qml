// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A section heading in Home's side panel ("Kite news", "Recent worlds"):
 * quieter than the page's own headings, in the heading face at title size.
 */
Text {
    elide: Text.ElideRight
    color: Theme.palette.textSecondary
    font.family: Theme.font.heading
    font.pixelSize: Theme.type.title.pixelSize
    font.weight: Theme.type.title.weight
}
