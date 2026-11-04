// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * Where friends play, in the launcher's words: a friend on a server gets the server's name when it is one of the
 * big ones Kite knows (FeaturedServers), else its address, and its live favicon and players from ServerPinger.
 * While `active` it asks the pinger about every server a friend is on; the pinger keeps one answer a minute for all
 * views. Not visible.
 */
QtObject {
    id: root

    // ServerPinger, or null.
    property var pinger: null
    // FeaturedServers.servers, or [].
    property var featured: []
    // FriendsController.friends, or [].
    property var friends: []
    // FriendsController.invites, or []: the servers friends invite you to are pinged too.
    property var invites: []
    // A view that shows places is on screen; only then are servers pinged.
    property bool active: false

    // The addresses friends play on or invite you to, each once.
    readonly property var addresses: {
        var seen = []
        var invited = root.invites.map((one) => ({ state: "playing", worldType: "server", host: one.host, port: one.port }))
        var places = root.friends.concat(invited)
        for (var i = 0; i < places.length; ++i) {
            var address = root.addressOf(places[i])
            if (address.length > 0 && seen.indexOf(address) < 0)
                seen.push(address)
        }
        return seen
    }

    function pingAll() {
        if (!root.active || !root.pinger)
            return
        for (var i = 0; i < root.addresses.length; ++i)
            root.pinger.ping(root.addresses[i])
    }
    onActiveChanged: root.pingAll()
    onAddressesChanged: root.pingAll()
    onPingerChanged: root.pingAll()

    // Where Join would go, or "" for a friend not on a server whose address they show.
    function addressOf(friend) {
        return !!friend && friend.state === "playing" && friend.worldType === "server" && !!friend.host
                ? Format.friendAddress(friend) : ""
    }

    // { address, name, favicon, online } for a friend on a server with a known address, ({}) otherwise. `name` is
    // the featured server's or the address; `favicon` "" until the server answers; `online` the players there, -1
    // while unknown or hidden. Reads the pinger's revision, so a binding follows every answer.
    function of(friend) {
        var address = root.addressOf(friend)
        if (address.length === 0)
            return ({})
        var status = root.pinger ? root.statusOf(address, root.pinger.revision) : ({})
        var live = status.state === "online"
        return {
            address: address,
            name: root.nameOf(friend, address),
            favicon: live && status.favicon ? status.favicon : "",
            online: live && status.online !== undefined ? status.online : -1
        }
    }

    // @p revision is only there so a binding that calls this reads it.
    function statusOf(address, revision) {
        return root.pinger.status(address)
    }

    function nameOf(friend, address) {
        if (root.pinger) {
            for (var i = 0; i < root.featured.length; ++i) {
                if (root.pinger.friendsOn(root.featured[i].address, [friend]).length > 0)
                    return root.featured[i].name
            }
        }
        return address
    }
}
