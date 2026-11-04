// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Another launcher's own icon, where Kite already carries it (Modrinth,
 * CurseForge, the Minecraft Launcher's grass block, ATLauncher, Technic, FTB);
 * a neutral glyph for the rest (Prism Launcher, MultiMC, PolyMC, MeshMC), whose
 * name always stands beside it.
 */
Item {
    id: root

    // LauncherImport::sourceKey(): "prism", "modrinth", "curseforge", ...
    property string launcher: ""
    property int size: Theme.icon.md

    readonly property string source: {
        switch (root.launcher) {
        case "modrinth": return "qrc:/icons/multimc/scalable/instances/modrinth.svg"
        case "curseforge": return "qrc:/icons/multimc/scalable/instances/flame.svg"
        case "official": return "qrc:/icons/multimc/48x48/minecraft.png"
        case "atlauncher": return "qrc:/icons/multimc/scalable/atlauncher.svg"
        case "technic": return "qrc:/icons/multimc/scalable/technic.svg"
        case "ftb": return "qrc:/icons/multimc/scalable/instances/ftb_logo.svg"
        default: return ""
        }
    }

    implicitWidth: root.size
    implicitHeight: root.size

    Image {
        anchors.fill: parent
        visible: root.source.length > 0
        source: root.source
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
    MeshIcon {
        anchors.centerIn: parent
        visible: root.source.length === 0
        iconName: "folder-input"
        size: root.size
        color: Theme.palette.textSecondary
    }
}
