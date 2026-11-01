// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * A MenuSeparator that can be hidden, like MenuEntry for a MenuItem.
 */
MenuSeparator {
    height: visible ? implicitHeight : 0
}
