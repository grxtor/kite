// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

T.Label {
    id: control

    // A sensible default that most call sites never touch; a call site
    // that does set font.pixelSize/weight itself overrides this, same as
    // any other declaratively-assigned default in QML.
    font.family: Theme.font.family
    font.pixelSize: Theme.type.body.pixelSize
    font.weight: Theme.type.body.weight

    color: control.enabled ? Theme.palette.textPrimary : Theme.palette.textDisabled
    linkColor: Theme.palette.accent
}
