// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * A friend's Join (the Friends page, Home's panel): the instance on the
 * friend's Minecraft version enters their server. Not visible; Main.qml
 * holds one and the pages ask it.
 *
 * `join(friend)`: one instance that can start joins at once; several ask
 * which (FriendJoinDialog); none offers to create one on that version, in
 * the New instance dialog (`createInstanceRequested`), or to try another
 * version. A running instance cannot be joined with (the game is already
 * up), so that is said instead of "joining". `tip(friend)` is the same
 * decision in words, for what Join's tooltip says it will start.
 *
 * `friend` is a friend map (FriendsController.friends), joinable
 * (Format.friendJoinable).
 */
Item {
    id: root

    // QmlShell, or null.
    property var shell: null
    // The instance picked in the play bar: it leads the candidates, as the
    // one most likely meant.
    property string selectedId
    // Main.qml's Toast.
    property var toast: null

    signal createInstanceRequested(string version)

    visible: false

    // What Join would work with for @p friend: { candidates (the instances
    // on the friend's version, the selected one first), ready (those that
    // can start), address }.
    function resolve(friend) {
        var candidates = root.selectedFirst(root.shell.instancesForVersion(friend.version))
        return {
            candidates: candidates,
            ready: candidates.filter((one) => one.canLaunch),
            address: Format.friendAddress(friend)
        }
    }
    // The instance that is in the way when none on the version can start:
    // the running one first.
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
    // What pressing Join on @p friend would do, named by the instance it
    // starts. Asked as the pointer arrives, so it is not an old answer.
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
    // `list` (instance maps) with the selected instance first.
    function selectedFirst(list) {
        var at = list.findIndex((one) => one.id === root.selectedId)
        return at <= 0 ? list : [list[at]].concat(list.slice(0, at), list.slice(at + 1))
    }
    function joinWith(id, instanceName, address) {
        // After the launch is asked for, and only when it starts (an
        // instance that cannot only comes to its Log tab): starting the
        // game may stop on a question (an offline player name) first, and
        // the words would have run out behind it.
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
