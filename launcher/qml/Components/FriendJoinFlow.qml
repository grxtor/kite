// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A friend's Join: the instance on the friend's Minecraft version enters their server. Not visible; Main.qml holds
 * one. `join(friend)`: one startable instance joins at once, several ask which (FriendJoinDialog), none offers to
 * create one or try another version. `tip(friend)` is the same decision in words.
 */
Item {
    id: root

    property var shell: null
    property string selectedId
    property var toast: null

    signal createInstanceRequested(string version)

    visible: false

    // { candidates (instances on the friend's version, selected first), ready (those that can start), address }.
    function resolve(friend) {
        var candidates = root.selectedFirst(root.shell.instancesForVersion(friend.version))
        return {
            candidates: candidates,
            ready: candidates.filter((one) => one.canLaunch),
            address: Format.friendAddress(friend)
        }
    }
    function stuckOf(candidates) {
        return candidates.find((one) => one.running) || candidates[0]
    }
    function join(friend) {
        if (!root.shell || !friend || !Format.friendJoinable(friend))
            return
        var found = root.resolve(friend)
        if (found.candidates.length > 0 && found.ready.length === 0) {
            var stuck = root.stuckOf(found.candidates)
            root.toast.show(stuck.running ? qsTr("%1 is already running. Quit it, then press Join.").arg(stuck.name)
                                          : qsTr("%1 can't start. Fix it, then press Join.").arg(stuck.name))
            return
        }
        if (found.ready.length === 1) {
            root.joinWith(found.ready[0].id, found.ready[0].name, found.address)
            return
        }
        pickerLoader.active = true
        var picker = pickerLoader.item
        picker.friendName = friend.name
        picker.instanceName = friend.instance || ""
        picker.version = friend.version
        picker.host = friend.host
        picker.address = found.address
        picker.candidates = found.candidates
        picker.others = found.candidates.length === 0 ? root.shell.otherInstances(friend.version) : []
        picker.open()
    }
    function tip(friend) {
        if (!root.shell || !friend)
            return ""
        var found = root.resolve(friend)
        if (found.candidates.length === 0)
            return qsTr("No Minecraft %1 instance yet").arg(friend.version)
        if (found.ready.length === 0) {
            var stuck = root.stuckOf(found.candidates)
            return stuck.running ? qsTr("%1 is already running").arg(stuck.name)
                                 : qsTr("%1 can't start").arg(stuck.name)
        }
        return found.ready.length === 1 ? qsTr("Start %1 and join %2").arg(found.ready[0].name).arg(friend.host)
                                        : qsTr("Choose an instance and join %1").arg(friend.host)
    }
    function selectedFirst(list) {
        var at = list.findIndex((one) => one.id === root.selectedId)
        return at <= 0 ? list : [list[at]].concat(list.slice(0, at), list.slice(at + 1))
    }
    function joinWith(id, instanceName, address) {
        // After the launch is asked for, and only when it starts: starting may stop on a question (an offline
        // player name) first, and the words would have run out behind it.
        if (root.shell.joinServer(id, address))
            root.toast.show(qsTr("Starting %2 to join %1…").arg(address).arg(instanceName))
    }

    Loader {
        id: pickerLoader
        active: false
        sourceComponent: FriendJoinDialog {
            onPicked: (id, name) => root.joinWith(id, name, address)
            onCreateRequested: (version) => root.createInstanceRequested(version)
        }
    }
}
