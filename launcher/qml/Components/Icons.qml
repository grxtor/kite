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
}
