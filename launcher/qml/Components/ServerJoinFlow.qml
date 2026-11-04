// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Joining a big server of the quick-join bar. Not visible; Main.qml holds one. `join(server)`: the selected (else
 * last played) instance enters it when its Minecraft is one the server takes; otherwise, and for
 * `joinWithAnother(server)`, ServerJoinDialog asks which. `instanceName(server)` is the one a click would start, for
 * the bar's words.
 */
Item {
    id: root

    property var shell: null
    // Main.dockInstanceId: the selection, else the most recently played instance.
    property string selectedId
    property var toast: null

    // A new instance for a server none fits; @p version is the newest it takes, "" for the default.
    signal createInstanceRequested(string version)

    visible: false

    // The server's versions in words: "1.8.x, 1.21.x, 26.x", "1.8 or newer", "1.8 to 1.12", "1.12 or older"; ""
    // for any.
    function rangeOf(server) {
        var lines = server.versions || []
        if (lines.length > 0)
            return lines.map((line) => line + ".x").join(", ")
        var low = server.minVersion || ""
        var high = server.maxVersion || ""
        if (low.length > 0 && high.length > 0)
            return qsTr("%1 to %2").arg(low).arg(high)
        if (low.length > 0)
            return qsTr("%1 or newer").arg(low)
        return high.length > 0 ? qsTr("%1 or older").arg(high) : ""
    }
    function instancesFor(server) {
        return root.shell ? root.shell.instancesForServer(server.minVersion || "", server.maxVersion || "",
                                                          server.versions || []) : []
    }
    function selectedIn(instances) {
        return instances.find((one) => one.id === root.selectedId) || null
    }
    function instanceName(server) {
        var chosen = root.selectedIn(root.instancesFor(server))
        return chosen && chosen.fits ? chosen.name : ""
    }
    function join(server) {
        if (!root.shell || !server)
            return
        var instances = root.instancesFor(server)
        var chosen = root.selectedIn(instances)
        if (!chosen || !chosen.fits) {
            root.ask(server, instances, chosen)
            return
        }
        if (!chosen.canLaunch) {
            root.toast.show(chosen.running ? qsTr("%1 is already running. Quit it, then press Join.").arg(chosen.name)
                                           : qsTr("%1 can't start. Fix it, then press Join.").arg(chosen.name))
            return
        }
        root.joinWith(chosen.id, chosen.name, server)
    }
    function joinWithAnother(server) {
        if (root.shell && server)
            root.ask(server, root.instancesFor(server), null)
    }
    function ask(server, instances, refused) {
        pickerLoader.active = true
        var picker = pickerLoader.item
        picker.server = server
        picker.range = root.rangeOf(server)
        picker.refused = refused && !refused.fits ? refused : null
        picker.instances = instances
        picker.open()
    }
    function joinWith(id, instanceName, server) {
        // Only once the launch is asked for and starts: it may stop on a question (an offline player name) first.
        if (root.shell.joinServer(id, server.address))
            root.toast.show(qsTr("Starting %2 to join %1…").arg(server.name).arg(instanceName))
    }

    Loader {
        id: pickerLoader
        active: false
        sourceComponent: ServerJoinDialog {
            onPicked: (id, name) => root.joinWith(id, name, server)
            onCreateRequested: root.createInstanceRequested(server.maxVersion || "")
        }
    }
}
