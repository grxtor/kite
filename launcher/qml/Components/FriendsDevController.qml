// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * A stand-in for FriendsController with made-up people, for looking at the friends views without a server
 * (Main.qml's dev route "friendsdemo=<scenario>"). It has what the views read and answers every action from its own
 * lists; nothing is sent anywhere. Scenarios: "full", "empty", "sent" (one request you sent, nobody else),
 * "noaccount", "off", "connecting", "error", "stale", "needsignin". `faceDir` holds <name>.png heads; without it
 * the heads are the placeholder.
 */
QtObject {
    id: root

    property string scenario: "full"
    property string faceDir: ""

    readonly property bool populated: ["full", "stale"].indexOf(root.scenario) >= 0
    property bool enabled: root.scenario !== "off"
    readonly property string state: root.scenario === "noaccount" ? "noAccount"
                                  : root.scenario === "connecting" ? "signingIn"
                                  : ["error", "stale", "needsignin"].indexOf(root.scenario) >= 0 ? "error"
                                  : root.enabled ? "ready" : "off"
    readonly property string error: root.scenario === "needsignin" ? "Your Minecraft sign-in has run out."
                                  : root.state === "error" ? "Kite's friends server did not answer." : ""
    readonly property bool needsSignIn: root.scenario === "needsignin"
    readonly property bool refused: false
    readonly property bool loaded: ["full", "stale", "empty", "sent"].indexOf(root.scenario) >= 0
    readonly property bool settingsLoaded: root.loaded
    readonly property double updatedAtMs: root.loaded ? Date.now() - 4 * 60 * 1000 : 0
    readonly property string myName: "GRXTOR"
    readonly property var settings: ({ visibility: "friends", hideServer: false, shareIpServers: false,
                                       allowJoin: true, allowInvites: true })
    readonly property var myStatus: ({ playing: false, worldType: "", host: "", serverHidden: false, joinable: false })
    readonly property bool live: root.state === "ready"
    readonly property var chat: null
    readonly property var outgoingInvites: []
    readonly property var inviteTarget: ({})

    property var friends: root.populated ? [
        root.friend("a1", "Dinnerbone", "playing", { worldType: "server", host: "mc.hypixel.net", port: 25565,
                                                      version: "1.21.4", joinable: true, unread: 2 }),
        root.friend("a2", "ardagoodman_", "playing", { worldType: "server", host: "play.example.net", port: 25565,
                                                        version: "1.20.1", joinOff: true }),
        root.friend("a3", "jeb_", "playing", { worldType: "singleplayer", version: "26.3" }),
        root.friend("a4", "LongestNameEver16", "online", { statusText: "In Kite" }),
        root.friend("a5", "Marc", "online", { statusText: "In Kite" }),
        root.friend("a6", "Grumm", "offline", { statusText: "Last seen 5 min ago" }),
        root.friend("a7", "Searge", "offline", { statusText: "Last seen yesterday" }),
        root.friend("a8", "xlson", "offline", { statusText: "Last seen 12 days ago" })
    ] : []
    property var incoming: root.populated ? [root.person("b1", "Notch"), root.person("b2", "kingbdogz")] : []
    property var outgoing: root.populated ? [root.person("c1", "slicedlime"), root.person("c2", "Kappische")]
                         : root.scenario === "sent" ? [root.person("c1", "ardagoodman_")] : []
    property var blocked: root.populated ? [root.person("d1", "Herobrine"), root.person("d2", "xX_Griefer_Xx")] : []
    property var invites: root.populated ? [{
        uuid: root.uuidOf("a1"), name: "Dinnerbone", face: root.faceOf("Dinnerbone"), host: "mc.hypixel.net",
        port: 25565, createdAt: new Date().toISOString(), expiresAt: new Date(Date.now() + 8 * 60000).toISOString(),
        expiresAtMs: Date.now() + 8 * 60000, addressKind: "domain"
    }] : []
    readonly property int onlineCount: root.friends.filter((one) => one.state !== "offline").length
    readonly property int incomingCount: root.incoming.length

    signal changed()
    signal updatedAtChanged()
    signal actionFinished(string action, bool ok, string message)

    function uuidOf(key) {
        return key + "000000-0000-4000-8000-000000000000"
    }
    function faceOf(name) {
        return root.faceDir.length > 0 ? "file://" + root.faceDir + "/" + name + ".png" : ""
    }
    function person(key, name) {
        return { uuid: root.uuidOf(key), name: name, face: root.faceOf(name), createdAt: new Date().toISOString() }
    }
    function friend(key, name, state, more) {
        var one = { uuid: root.uuidOf(key), name: name, face: root.faceOf(name), state: state, since: "",
                    lastSeenAt: "", instance: "", version: "", worldType: "", host: "", port: 0, joinable: false,
                    joinOff: false, statusText: "", unread: 0 }
        return Object.assign(one, more)
    }
    function without(list, uuid) {
        return list.filter((one) => one.uuid !== uuid)
    }
    function nameOf(uuid) {
        var all = root.friends.concat(root.incoming, root.outgoing, root.blocked)
        var found = all.find((one) => one.uuid === uuid)
        return found ? found.name : "this player"
    }
    // As the real one: the answer comes after the caller has returned.
    function finish(action, ok, message) {
        Qt.callLater(() => {
            root.changed()
            root.actionFinished(action, ok, message)
        })
    }

    function refresh() { root.finish("refresh", root.state === "ready", root.error) }
    function refreshBlocked() { root.finish("refreshBlocked", true, "") }
    function sendRequest(name) {
        name = name.trim()
        if (name.toLowerCase() === root.myName.toLowerCase())
            return root.finish("sendRequest", false, "That is your own name")
        if (root.friends.some((one) => one.name.toLowerCase() === name.toLowerCase()))
            return root.finish("sendRequest", false, name + " is already your friend")
        root.outgoing = root.outgoing.concat([root.person("e" + root.outgoing.length, name)])
        root.finish("sendRequest", true, "Friend request sent to " + name)
    }
    function accept(uuid) {
        var name = root.nameOf(uuid)
        root.incoming = root.without(root.incoming, uuid)
        root.friends = root.friends.concat([root.friend("f" + root.friends.length, name, "online",
                                                        { statusText: "In Kite" })])
        root.finish("accept", true, name + " is now your friend")
    }
    function decline(uuid) {
        root.incoming = root.without(root.incoming, uuid)
        root.finish("decline", true, "Request declined")
    }
    function cancelRequest(uuid) {
        root.outgoing = root.without(root.outgoing, uuid)
        root.finish("cancelRequest", true, "Request cancelled")
    }
    function removeFriend(uuid) {
        var name = root.nameOf(uuid)
        root.friends = root.without(root.friends, uuid)
        root.finish("removeFriend", true, name + " is no longer your friend")
    }
    function block(uuid) {
        var name = root.nameOf(uuid)
        root.friends = root.without(root.friends, uuid)
        root.incoming = root.without(root.incoming, uuid)
        root.blocked = root.blocked.concat([{ uuid: uuid, name: name, face: root.faceOf(name) }])
        root.finish("block", true, name + " is blocked")
    }
    function unblock(uuid) {
        var name = root.nameOf(uuid)
        root.blocked = root.without(root.blocked, uuid)
        root.finish("unblock", true, name + " is unblocked")
    }
    function sendInvite(uuid, address) { root.finish("sendInvite", true, "Invite sent") }
    function cancelInvite(uuid) { root.finish("cancelInvite", true, "") }
    function dismissInvite(uuid) {
        root.invites = root.without(root.invites, uuid)
        root.finish("dismissInvite", true, "")
    }
}
