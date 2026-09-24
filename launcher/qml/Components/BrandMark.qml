// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * MeshMC's own mark at any square size, rasterised at 2x so it stays crisp
 * on HiDPI. Set `size`; the caller positions it.
 */
Image {
    property int size: 24

    width: size
    height: size
    source: "qrc:/icons/multimc/scalable/instances/meshmc.svg"
    sourceSize: Qt.size(size * 2, size * 2)
    fillMode: Image.PreserveAspectFit
}
