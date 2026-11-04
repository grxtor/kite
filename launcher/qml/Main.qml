// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme
import Kite.Components

ApplicationWindow {
    id: root

    required property var instanceModel
    required property var selection
    // QmlShell: actions, account summary, extra instance models.
    required property var shell

    readonly property string moduleName: "Kite"

    width: 1240
    height: 780
    minimumWidth: 860
    minimumHeight: 540
    title: "Kite"
    color: Theme.palette.canvas

    // Set from QmlShell::rootProperties(); false when Skin3D was not built.
    property bool skin3DAvailable: false

    // Native Cocoa only; elsewhere Qt would draw the bar inside the window.
    readonly property bool nativeMenuBar: Qt.platform.os === "osx" && Qt.platform.pluginName === "cocoa"
                                          && SettingsStore.bool("MacNativeMenuBar")
    menuBar: AppMenuBar {
        visible: root.nativeMenuBar
        enabled: !root.onboarding
        about: root.shellValue("about")
        updaterAvailable: !!root.shellValue("updaterAvailable")
        onNewInstanceRequested: (mode) => root.openNewInstance(mode)
        onNavigateRequested: (target) => root.navigate(target)
        onCheckForUpdatesRequested: root.call("checkForUpdates")
    }

    property string selectedId: ""
    // "home", "discover", "settings", "instance", "accounts" or "friends".
    property string page: "home"
    property string openedInstanceId: ""
    property var openedDetails: null

    function openInstance(id) {
        if (!root.shell || typeof root.shell.instanceDetails !== "function")
            return
        root.openedDetails = root.shell.instanceDetails(id)
        root.openedInstanceId = id
        root.selectedId = id
        root.page = "instance"
    }
    // Home opens an instance on its settings, the page people want there.
    function openInstanceSettings(id) {
        root.openInstance(id)
        if (instancePage)
            instancePage.tab = "settings"
    }

    Binding {
        target: LauncherStore
        property: "iconRevision"
        value: root.shellValue("iconRevision", 0)
    }

    Component.onCompleted: {
        if (root.shell && root.shell.settings) {
            SettingsStore.adapter = root.shell.settings
            JavaStore.controller = root.shell.javaController
            JavaStore.download = root.shell.javaDownload
            LauncherStore.folders = root.shell.folderSettings
            LauncherStore.updates = root.shell.updateSettings
            LauncherStore.catPacks = root.shell.catPacks
            LauncherStore.icons = root.shell
            var mode = SettingsStore.string("UiThemeMode")
            if (mode === "dark" || mode === "light" || mode === "system")
                Theme.mode = mode
            var scheme = SettingsStore.string("UiPalette")
            if (scheme === "amethyst" || scheme === "ember" || scheme === "diamond" || scheme === "grass")
                Theme.scheme = scheme
            var look = SettingsStore.string("UiLook")
            if (look === "stage" || look === "calm" || look === "night" || look === "classic")
                Theme.look = look
            // A window too narrow for the panel overrides the saved choice for this session only.
            root.friendsPanelOpen = SettingsStore.bool("FriendsPanelOpen")
                                    && root.width >= root.friendsPanelMinWindow
            root.friendsPanelExpanded = SettingsStore.bool("FriendsPanelExpanded")
            root.friendsPanelWidth = SettingsStore.number("FriendsPanelWidth")
            root.sidebarWidth = SettingsStore.number("UiSidebarWidth")
        }
        onboardingLoader.active = !!root.shell && !!root.shell.setupSteps && root.shell.setupSteps.length > 0
        if (root.devRoute.length > 0)
            Qt.callLater(root.applyDevRoute)
    }

    // KITE_QML_ROUTE: ";"-separated steps run at start-up for review snapshots,
    // e.g. "theme=light;scheme=ember;size=1400x900;page=settings;section=java"
    // or "instance=<id>;tab=mods". "nav=<path>" goes through navigate().
    // "accounts-demo" is read by AccountsController, not here.
    property string devRoute: ""
    property string devJoinInvite: ""
    property string devInviteSend: ""
    property var devShareSteps: []
    Connections {
        target: root.devInviteSend.length > 0 ? root.friendsController : null
        function onChanged() {
            var friend = root.friendsController.friends.find((one) => one.name === root.devInviteSend)
            if (friend) {
                root.devInviteSend = ""
                invitePicker.invite(friend.uuid)
            }
        }
    }
    Connections {
        target: root.devShareSteps.length > 0 ? root.shareCodes : null
        function onPrepared(id) {
            if (root.devShareSteps.indexOf("make") >= 0)
                root.shareCodes.share(id)
        }
        function onLookedUp() {
            if (root.devShareSteps.indexOf("create") >= 0)
                Qt.callLater(() => root.dialog(newInstanceLoader).createFromCode())
        }
    }
    Connections {
        target: root.devJoinInvite.length > 0 ? root.friendsController : null
        function onChanged() {
            var invite = root.friendsController.invites.find((one) => one.name === root.devJoinInvite)
            if (invite) {
                root.devJoinInvite = ""
                friendJoin.joinInvite(invite)
            }
        }
    }
    function applyDevRoute() {
        // Routes reach into pages the route may not show; build them all.
        for (var slot of [settingsLoader, discoverLoader, instanceLoader, accountsLoader, friendsLoader])
            slot.built = true
        var steps = root.devRoute.split(";")
        for (var i = 0; i < steps.length; ++i) {
            var eq = steps[i].indexOf("=")
            var key = (eq < 0 ? steps[i] : steps[i].slice(0, eq)).trim()
            var value = eq < 0 ? "" : steps[i].slice(eq + 1).trim()
            if (key === "theme")
                Theme.mode = value
            else if (key === "scheme")
                Theme.scheme = value
            else if (key === "look")
                Theme.look = value
            else if (key === "size") {
                var wh = value.split("x")
                root.width = parseInt(wh[0])
                root.height = parseInt(wh[1])
            } else if (key === "page")
                root.page = value
            else if (key === "instance")
                root.openInstance(value)
            else if (key === "tab")
                instancePage.tab = value
            else if (key === "section")
                settingsPage.section = value
            else if (key === "select")
                root.selectedId = value
            else if (key === "detail")
                devDetail.start()
            else if (key === "newinstance")
                root.openNewInstance(value)
            // After newinstance: loader=<kind> (or "performance") picks the type, mods opens the mod picker.
            else if (key === "loader")
                root.dialog(newInstanceLoader).pickKind(value)
            else if (key === "mods")
                root.dialog(newInstanceLoader).openModPicker()
            else if (key === "nav")
                root.navigate(value)
            else if (key === "import")
                root.openImport(value)
            else if (key === "friends")
                Qt.callLater(root.openFriends, "friends")
            // friendsTab: "requests" or "blocked".
            else if (key === "friendsTab")
                Qt.callLater(root.openFriends, value)
            // friendspanel=closed|strip|open; friendspanelwidth in px. Both are saved.
            else if (key === "friendspanel") {
                root.setFriendsPanelOpen(value !== "closed")
                if (value === "strip" || value === "open")
                    root.setFriendsPanelExpanded(value === "open")
            } else if (key === "friendspanelwidth")
                root.setFriendsPanelWidth(parseInt(value))
            else if (key === "profilemenu")
                Qt.callLater(() => sidebar.openProfileMenu())
            else if (key === "picker")
                Qt.callLater(() => playDock.openPicker())
            // launchmenu=playAs|profiler also opens that submenu.
            else if (key === "launchmenu")
                Qt.callLater(playDock.openLaunchMenu, value)
            // Play bar mid-launch without a launch: "launching=<stage>:<0..1>" (-1 for unknown progress).
            else if (key === "launching") {
                var launchArgs = value.length > 0 ? value.split(":") : ["Downloading assets", "0.42"]
                playDock.previewLaunch = {
                    status: launchArgs[0],
                    progress: launchArgs.length > 1 ? parseFloat(launchArgs[1]) : -1
                }
            }
            // export=<id>[:<format>]
            else if (key === "export") {
                var exportArgs = value.split(":")
                Qt.callLater(homePage.exportInstance, exportArgs[0], exportArgs[1] || "")
            }
            else if (key === "shortcut")
                Qt.callLater(homePage.createShortcut, value)
            else if (key === "librarymenu")
                Qt.callLater(homePage.openInstanceMenu, value)
            else if (key === "groupmenu")
                Qt.callLater(homePage.openGroupMenu, value)
            else if (key === "duplicate")
                Qt.callLater(homePage.duplicateInstance, value)
            // share=<id>: "Share as code" for that instance.
            else if (key === "share")
                Qt.callLater(homePage.shareInstance, value, value)
            // After share=<id>: sharemake makes the code once the instance is read.
            else if (key === "sharemake")
                root.devShareSteps = root.devShareSteps.concat(["make"])
            // After newinstance=code: sharecode=<code> types and looks up a code; sharecreate then creates it.
            else if (key === "sharecode")
                Qt.callLater((code) => root.dialog(newInstanceLoader).enterShareCode(code), value)
            else if (key === "sharelist")
                Qt.callLater(() => root.dialog(newInstanceLoader).showShareContents())
            else if (key === "sharecreate")
                root.devShareSteps = root.devShareSteps.concat(["create"])
            // invite=<address>: the friend picker for that server.
            else if (key === "invite")
                Qt.callLater(invitePicker.openFor, { name: value, address: value })
            // After invite=: invitesend=<name> presses that friend's Invite once the list is read.
            else if (key === "invitesend")
                root.devInviteSend = value
            // joininvite=<name>: Join on that friend's invite, once it has arrived.
            else if (key === "joininvite")
                root.devJoinInvite = value
            // The next keys act on the instance page, after instance=<id>.
            else if (key === "instancemenu")
                Qt.callLater(instancePage.openHeaderMenu)
            else if (key === "exportlist")
                Qt.callLater(instancePage.openContentExportList)
            else if (key === "browsefilters")
                Qt.callLater(instancePage.openBrowseFilters)
            else if (key === "bundles")
                Qt.callLater(instancePage.review, "bundles")
            else if (key === "perfdialog")
                Qt.callLater(instancePage.review, "perfdialog")
            // review=versionmore|newcomponent|otherlogs|logfind|shotselect|shotrename
            else if (key === "review")
                Qt.callLater(instancePage.review, value)
            // ask=pickAccount|busySkip: questions only a real launch asks.
            else if (key === "ask")
                root.call("debugShowRequest", value)
            // sidebar=collapsed is the rail, anything else the labelled sidebar. Both are saved.
            else if (key === "sidebar")
                SettingsStore.setValue("UiSidebarCollapsed", value === "collapsed")
            else if (key === "sidebarwidth")
                root.setSidebarWidth(parseInt(value))
            // Every first-run step; nothing is saved unless one is finished. onboarding=<step>
            // (language|java|account|done) opens it on that step.
            else if (key === "onboarding") {
                onboardingLoader.active = true
                onboardingLoader.item.steps = ["language", "java", "account", "done"]
                onboardingLoader.item.index = Math.max(0, onboardingLoader.item.steps.indexOf(value))
                onboardingLoader.item.active = true
            }
            // After instance=<id>: dragover=<file>[,<file>] shows the drop overlay as if those files were dragged
            // over the page; drop=<file>[,<file>] drops them on it.
            else if (key === "dragover")
                Qt.callLater(windowDrop.preview, value.split(","))
            else if (key === "drop")
                Qt.callLater(windowDrop.dropFiles, value.split(","))
            // toast=<text>: a toast with an action, to see how a long message wraps.
            else if (key === "toast")
                Qt.callLater(() => toast.show(value, "success", "Open", () => {}))
            // link=<url>: the question a link from the network asks before it opens.
            else if (key === "link")
                Qt.callLater(() => ExternalLinks.open(root.contentItem, value))
            // serverjoin=<featured id>: the instance picker of quick join.
            else if (key === "serverjoin")
                Qt.callLater(() => serverJoin.joinWithAnother(root.featuredList.find((one) => one.id === value)))
            else if (key === "crashed")
                root.call("debugMarkInstanceCrashed", value)
            // crashdoctor=<id>: the crash doctor reads that instance's last run's files, as a crash
            // read again later; crashwindow=<id> as one that just happened, which opens the window.
            else if (key === "crashdoctor")
                root.call("debugAnalyseCrash", value)
            else if (key === "crashwindow")
                root.call("debugCrashNow", value)
            // crashsend=<id>: sends that crash to Kite as Send would; crashrules=<file>: newer crash rules.
            else if (key === "crashsend")
                root.call("debugSendCrash", value)
            else if (key === "crashrules")
                root.call("debugUseCrashRules", value)
            // click=x,y[,right[,delayMs]]: a real click at a window point.
            else if (key === "click")
                root.call("debugClick", value)
            else if (key === "iconpicker") {
                var iconDialog = root.dialog(iconLoader)
                iconDialog.targetId = root.dockInstanceId
                iconDialog.current = value.length > 0 ? value : "grass"
                iconDialog.open()
            }
        }
    }
    Timer {
        id: devDetail
        interval: 3500
        onTriggered: {
            var parts = root.devRoute.match(/detail=(\d+)/)
            discoverPage.openResult(parts ? parseInt(parts[1]) : 0)
        }
    }
    onSelectedIdChanged: {
        if (selectedId.length > 0)
            root.selection.selectOnly(selectedId)
        else
            root.selection.clear()
    }

    // UiSidebarWidth: read at start-up, written when the sidebar commits a width.
    property int sidebarWidth: 240
    function setSidebarWidth(width) {
        root.sidebarWidth = width
        SettingsStore.setValue("UiSidebarWidth", width)
    }

    // Null where this build has no friends.
    readonly property var friendsController: root.shellValue("friends")
    readonly property var shareCodes: root.shellValue("shareCodes")
    // A window narrower than friendsPanelMinWindow starts without the panel and
    // leaves the FriendsPanelOpen setting alone; the toggle still opens it.
    // FriendsPanelOpen/Expanded/Width are read at start-up and written on change.
    readonly property int friendsPanelMinWindow: 1100
    property bool friendsPanelOpen: false
    property bool friendsPanelExpanded: false
    property int friendsPanelWidth: 320
    readonly property bool friendsPanelShown: !!root.friendsController && root.friendsPanelOpen
    // Where friends play, shared by every friends view; their servers are pinged while one of them is on screen.
    readonly property FriendPlaces friendPlaces: FriendPlaces {
        pinger: root.serverPinger
        featured: root.featuredList
        friends: root.friendsController ? root.friendsController.friends : []
        invites: root.friendsController ? root.friendsController.invites : []
        active: Format.friendsPhase(root.friendsController) === "ready"
                && (root.friendsPanelShown || root.page === "friends" || root.page === "home")
    }
    function setFriendsPanelOpen(open) {
        root.friendsPanelOpen = open
        SettingsStore.setValue("FriendsPanelOpen", open)
    }
    function setFriendsPanelExpanded(expanded) {
        root.friendsPanelExpanded = expanded
        SettingsStore.setValue("FriendsPanelExpanded", expanded)
    }
    // The conversation with @p uuid in the friends panel, opened (and opened out) for it.
    function openFriendChat(uuid) {
        root.setFriendsPanelOpen(true)
        friendsPanel.openChat(uuid)
    }
    function setFriendsPanelWidth(width) {
        root.friendsPanelWidth = width
        SettingsStore.setValue("FriendsPanelWidth", width)
    }
    // tab is "friends", "requests", "blocked" or "add" (the add field focused).
    function openFriends(tab) {
        root.page = "friends"
        friendsPage.tab = tab === "requests" || tab === "blocked" ? tab : "friends"
        if (tab === "add")
            Qt.callLater(friendsPage.focusAddField)
    }

    // FriendJoinFlow: a new instance on a friend's Minecraft version.
    function createInstanceOn(version) {
        root.openNewInstance("create")
        root.shell.newInstance.selectMinecraftVersion(version)
    }

    function dialog(loader) {
        loader.active = true
        return loader.item
    }
    readonly property QtObject newInstanceDialog: newInstanceLoader.item

    // mode is "create" (default), "import" or "code"; source pre-fills the import field.
    function openNewInstance(mode, source) {
        if (!root.shell || !root.shell.newInstance)
            return
        var instanceDialog = root.dialog(newInstanceLoader)
        instanceDialog.controller = root.shell.newInstance
        instanceDialog.mode = mode === "import" || mode === "code" ? mode : "create"
        instanceDialog.presetSource = source || ""
        instanceDialog.open()
    }

    // An import from outside the window (`kite --import`, a second launcher).
    // An open dialog takes the new source in place unless it is still busy.
    function openImport(source) {
        if (!newInstanceDialog || !newInstanceDialog.opened) {
            root.openNewInstance("import", source)
            return
        }
        if (newInstanceDialog.creating) {
            toast.show(qsTr("Another instance is still being set up. Try again once it is in your library."))
            return
        }
        newInstanceDialog.mode = "import"
        newInstanceDialog.setImportSource(source)
    }

    /*
     * Cross-page links. Targets are paths, whole or split in two
     * (navigate("settings", "java") is navigate("settings/java")):
     *   "home", "discover", "accounts"
     *   "friends" or "friends/<tab>"
     *   "settings" or "settings/<section>"
     *   "instance/<id>" or "instance/<id>/<tab>"
     * Instance ids are folder names and never contain "/". Returns false and
     * leaves the page alone for an unknown path or instance.
     */
    function navigate(page, sub) {
        var path = sub === undefined || sub === null || String(sub).length === 0
                ? String(page) : page + "/" + sub
        var parts = path.split("/")
        switch (parts[0]) {
        case "home":
        case "discover":
        case "accounts":
            root.page = parts[0]
            return true
        case "friends":
            root.openFriends(parts.length > 1 ? parts[1] : "")
            return true
        case "settings":
            root.page = "settings"
            if (parts.length > 1 && parts[1].length > 0)
                settingsPage.section = parts[1]
            return true
        case "instance":
            if (parts.length < 2 || parts[1].length === 0 || !root.shell
                    || typeof root.shell.instanceDetails !== "function"
                    || !root.shell.instanceDetails(parts[1]))
                return false
            root.openInstance(parts[1])
            if (parts.length > 2 && parts[2].length > 0 && instancePage)
                instancePage.tab = parts[2]
            return true
        }
        return false
    }

    // With no account, go to Accounts instead of the launch flow's question.
    function launch(id) {
        if (root.sendToAccountsIfNone())
            return
        root.call("launchInstance", id)
    }
    function sendToAccountsIfNone() {
        if (!root.shell || root.shell.accountCount !== 0)
            return false
        root.navigate("accounts")
        toast.show(qsTr("Sign in with the Microsoft account that owns Minecraft to play."))
        return true
    }

    // LaunchOptionsMenu actions; mode is "offline" or "demo".
    function launchMode(id, mode) {
        if (root.sendToAccountsIfNone())
            return
        root.shellResult("launchInstanceMode", id, mode)
    }
    function launchAs(id, accountId) {
        root.shellResult("launchInstanceAs", id, accountId)
    }
    function setProfiler(id, profilerKey) {
        root.shellResult("setInstanceProfiler", id, profilerKey)
    }
    function setPerformance(id, enabled) {
        root.shellResult("setPerformanceEnabled", id, enabled)
    }
    function launchOptions(id) {
        return root.shellResult("launchOptions", id) || ({})
    }

    // The Play options menu's choices, wherever a page or the play bar offers it (`target`).
    component LaunchRouting: Connections {
        function onLaunchModeRequested(id, mode) { root.launchMode(id, mode) }
        function onLaunchAsRequested(id, accountId) { root.launchAs(id, accountId) }
        function onProfilerChosen(id, profilerKey) { root.setProfiler(id, profilerKey) }
        function onPerformanceChosen(id, enabled) { root.setPerformance(id, enabled) }
        function onNavigateRequested(target) { root.navigate(target) }
    }
    LaunchRouting { target: homePage }
    LaunchRouting { target: root.instancePage }
    LaunchRouting { target: playDock }

    // The instance page has no bar: its header carries Play.
    function dockVisibleFor(page) {
        return page === "home" || page === "discover" || page === "settings" || page === "accounts"
               || page === "friends"
    }
    // While "Add content" is open, its Install is the primary action.
    readonly property bool dockSuppressed: root.page === "instance" && !!instancePage && instancePage.browsing === true
    // On Home while the hero's Play is on screen the bar would be a second Play.
    readonly property bool dockHeldByHero: pageStack.currentIndex === pageHost.pageOrder.indexOf("home")
                                           && homePage.heroPlayInView

    // Built on demand (PageSlot), null until first shown. Setting root.page
    // builds the page, so set the page first, then hand it values.
    readonly property Item settingsPage: settingsLoader.item
    readonly property Item discoverPage: discoverLoader.item
    readonly property Item instancePage: instanceLoader.item
    readonly property Item accountsPage: accountsLoader.item
    readonly property Item friendsPage: friendsLoader.item

    function instanceGroups() {
        var groups = root.shellValue("groups", [])
        return [""].concat(groups.filter(g => g.length > 0))
    }

    function call(name, arg) {
        if (root.shell && typeof root.shell[name] === "function")
            arg === undefined ? root.shell[name]() : root.shell[name](arg)
    }

    // A property of the shell, or `fallback` (null) where it is missing or empty.
    function shellValue(name, fallback) {
        var value = root.shell ? root.shell[name] : undefined
        return value ? value : (fallback === undefined ? null : fallback)
    }
    // What a shell method returns for up to two arguments; null where this build lacks the method.
    function shellResult(name, first, second) {
        if (!root.shell || typeof root.shell[name] !== "function")
            return null
        switch (arguments.length) {
        case 1: return root.shell[name]()
        case 2: return root.shell[name](first)
        default: return root.shell[name](first, second)
        }
    }
    function sectionModel(group) { return root.shellResult("sectionModel", group) }
    function exportController(id) { return root.shellResult("exportController", id) }
    function shortcutController(id) { return root.shellResult("shortcutController", id) }
    function pluginSurfaces(anchor, instanceId) { return root.shellResult("pluginSurfaces", anchor, instanceId) }

    // The shell's models and host, null where this build has none.
    readonly property var recentModel: root.shellValue("recentModel")
    readonly property var heroModel: root.shellValue("heroModel")
    readonly property var instancePageModel: root.shellValue("instancePageModel")
    readonly property var accountsController: root.shellValue("accountsController")
    readonly property var uiHost: root.shellValue("uiHost")
    // Quick join: the big servers (FeaturedServers) and their live status (ServerPinger).
    readonly property var featuredServers: root.shellValue("featuredServers")
    readonly property var serverPinger: root.shellValue("serverPinger")
    readonly property var featuredList: root.featuredServers ? root.featuredServers.servers : []

    // Ctrl+F searches the library on Home; from another page it goes Home
    // first and focuses the field after the page swap (pageTransition). The
    // instance page keeps its own Ctrl+F (LogTab).
    property bool focusHomeSearch: false
    // First-run setup covers the window: the shortcuts and the menu bar wait.
    readonly property bool onboarding: !!onboardingLoader.item && onboardingLoader.item.active === true
    Shortcut {
        sequences: [StandardKey.Find]
        enabled: root.page !== "instance" && !root.onboarding
        onActivated: {
            if (root.page === "home") {
                homePage.focusSearch()
            } else {
                root.focusHomeSearch = true
                root.page = "home"
            }
        }
    }
    Connections {
        target: JavaStore
        function onNavigateRequested(target) {
            root.navigate(target)
        }
    }

    Connections {
        target: root.shell
        ignoreUnknownSignals: true
        function onOpenInstanceLog(id) {
            root.openInstance(id)
            if (instancePage)
                instancePage.tab = "log"
        }
        function onImportRequested(source) {
            root.openImport(source)
        }
        function onNavigateRequested(target) {
            root.navigate(target)
        }
        // From C++ (QmlShell::performanceUnmatchedFound()): Kite Performance
        // found no version of these mods for the instance's new Minecraft.
        // A toast holds one line, so it names the mods and the button leads
        // to where they are removed or updated.
        // From C++ (QmlShell::crashAnalysed()): the crash doctor read why instance @p id crashed.
        // A crash that just happened opens the "why" window once; one read again later (the Log
        // tab's own analysis) does not. The Log tab's strip says it afterwards.
        function onCrashAnalysed(id, cause, fresh) {
            if (fresh)
                root.dialog(crashWindowLoader).show(id)
        }
        function onPerformanceUnmatchedFound(id, names) {
            toast.show(qsTr("No version of %1 for this Minecraft yet. The game may not start.").arg(names.join(", ")),
                       "danger", qsTr("Show mods"), function () {
                root.navigate("instance/" + id + "/content")
            })
        }
    }

    Binding {
        target: root.instancePageModel
        property: "instanceId"
        value: root.openedInstanceId.length > 0 ? root.openedInstanceId : "/"
        when: !!root.instancePageModel
    }

    // The selection, else the most recently played instance, else the first.
    readonly property string dockInstanceId: root.selectedId.length > 0 ? root.selectedId
            : (root.recentModel && root.recentModel.firstId ? root.recentModel.firstId
            : (root.instanceModel && root.instanceModel.firstId ? root.instanceModel.firstId : ""))

    Binding {
        target: root.heroModel
        property: "instanceId"
        value: root.dockInstanceId
        when: !!root.heroModel
    }

    function askRename(id, name) {
        var renameDialog = root.dialog(renameLoader)
        renameDialog.targetId = id
        renameDialog.value = name
        renameDialog.open()
    }
    function askIcon(id, iconKey) {
        var iconDialog = root.dialog(iconLoader)
        iconDialog.targetId = id
        iconDialog.current = iconKey
        iconDialog.open()
    }
    function askGroup(id, group) {
        var groupDialog = root.dialog(groupLoader)
        groupDialog.targetId = id
        groupDialog.value = group
        groupDialog.suggestions = root.instanceGroups()
        groupDialog.open()
    }
    function askDelete(id, name) {
        var deleteDialog = root.dialog(deleteLoader)
        deleteDialog.targetId = id
        deleteDialog.text = qsTr("Delete \u201c%1\u201d? Its folder \u2014 worlds, mods, screenshots \u2014 is removed for good.").arg(name)
        deleteDialog.open()
    }

    Shortcut {
        sequences: [StandardKey.New]
        enabled: !root.onboarding
        onActivated: root.openNewInstance()
    }

    ContentDropFlow {
        id: dropFlow
        onNotify: (text, tone) => toast.show(text, tone)
        onImportRequested: (source) => root.openImport(source)
    }

    function dropUrls(urls) {
        const list = []
        for (let i = 0; i < urls.length; ++i)
            list.push(urls[i].toString())
        return list
    }
    // A tile of the library or the sidebar took a drop (InstanceDropTarget).
    function dropOnInstance(id, urls) {
        if (root.shell && typeof root.shell.contentDrop === "function")
            dropFlow.handle(root.shell.contentDrop(id), urls, null)
    }

    // Files and links dropped on the window. On an instance's page they are added to it by what they are (mods,
    // packs, worlds; ContentDrop); anywhere else a zip, .mrpack or link opens import. Declared first, so
    // NewInstanceDialog's and the library's drop areas above it keep their drops.
    DropArea {
        id: windowDrop
        anchors.fill: parent
        keys: ["text/uri-list"]

        readonly property bool onInstance: root.page === "instance" && !!root.openedDetails
                                           && root.openedDetails.isMinecraft && !!root.openedDetails.drop
        // What the drag in progress holds, for the overlay (ContentDrop.preview()).
        property var rows: []
        property string unknown: ""
        // Dev route "dragover": the overlay stays up without a real drag.
        property bool previewing: false

        function show(paths) {
            const found = windowDrop.onInstance ? root.openedDetails.drop.preview(paths) : ({ rows: [], unknown: "" })
            windowDrop.rows = found.rows
            windowDrop.unknown = found.unknown
        }
        function preview(paths) {
            windowDrop.show(paths)
            windowDrop.previewing = true
        }
        // Dev route "drop": what a drop of these local files would do.
        function dropFiles(paths) {
            windowDrop.previewing = false
            if (windowDrop.onInstance)
                dropFlow.handle(root.openedDetails.drop, paths, instanceLoader.item ? instanceLoader.item.needsLoaderFirst : null)
        }

        onEntered: (drag) => windowDrop.show(drag.hasUrls ? root.dropUrls(drag.urls) : [])
        onDropped: (drop) => {
            if (!drop.hasUrls || drop.urls.length === 0)
                return
            const urls = root.dropUrls(drop.urls)
            drop.accept(Qt.CopyAction)
            if (windowDrop.onInstance)
                dropFlow.handle(root.openedDetails.drop, urls,
                                instanceLoader.item ? instanceLoader.item.needsLoaderFirst : null)
            else
                root.openImport(urls[0])
        }
    }

    // The sidebar, top bar and play bar are one surface (shellColor); the page
    // sits in an inset panel on the canvas. The panel adds no padding: each
    // page pads itself and scrolls up to the panel's top edge.

    // Opaque, so the panel's corner matte matches it exactly.
    readonly property color shellColor: Qt.tint(Theme.palette.canvas, Theme.glass.fill)
    // Gece: SkyBackdrop fills the window; the shell and both panels stay
    // transparent over it, without the corner matte.
    readonly property bool skyLook: Theme.look === "night"
    readonly property int panelRadius: Theme.radius.xl
    // The page column's margin on each side, shared by the top bar and the play bar so both line up
    // with the pages: content stops at Theme.layout.maxContentWidth and centres.
    readonly property int pageSideMargin: Math.max(Theme.space.xl + Theme.space.xs,
                                                   Math.round((panel.width - Theme.layout.maxContentWidth) / 2))
    // macOS: the title bar lies over the shell (QmlShell, MacWindowChrome).
    // Width of the traffic lights and the bar height that centres them; 0
    // elsewhere. Full screen hides them.
    property int windowControlsWidth: 0
    property int titleBarHeight: 0
    readonly property int windowControlsInset: root.visibility === Window.FullScreen ? 0 : root.windowControlsWidth
    // macOS: AppKit decides the height that centres the traffic lights.
    readonly property int shellBarHeight: root.titleBarHeight > 0 ? root.titleBarHeight : Theme.control.heightLg

    SkyBackdrop {
        anchors.fill: parent
        visible: root.skyLook
        barHeight: root.shellBarHeight
    }

    Rectangle {
        anchors.fill: parent
        color: root.skyLook ? "transparent" : root.shellColor

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.glass.highlight
        }
    }

    // Under the rail and the top bar, so their buttons still take clicks.
    WindowDragBand {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.shellBarHeight
        enabled: root.windowControlsInset > 0
    }

    // No Library or Friends entry: the library is on Home, the friends in the
    // right-edge panel.
    readonly property var navItems: [
        { id: "home", icon: "home", label: qsTr("Home") },
        { id: "discover", icon: "compass", label: qsTr("Discover") }
    ]

    SidebarNav {
        id: sidebar
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        bandHeight: root.shellBarHeight
        controlsWidth: root.windowControlsInset
        surround: root.shellColor
        // A narrow window always keeps the rail and offers no toggle.
        canExpand: root.width >= 1000
        collapsed: !canExpand || SettingsStore.bool("UiSidebarCollapsed")
        expandedWidth: root.sidebarWidth
        // Leaves the page area's least width and, with friends, the panel's
        // narrowest opened-out width, so dragging never squeezes either.
        roomWidth: root.width - friendsPanel.minPageWidth
                   - (root.friendsController ? friendsPanel.minExpandedWidth : 0)
        onWidthCommitted: (labelledWidth) => root.setSidebarWidth(labelledWidth)
        items: root.navItems
        footerItems: [
            { id: "settings", icon: "settings", label: qsTr("Settings") }
        ]
        // Pages the rail lacks light where they are reached from: Home for the
        // library and instances. Friends lights nothing (empty id).
        currentId: root.navItems.some(item => item.id === root.page)
                   || root.page === "settings" || root.page === "accounts" ? root.page
                   : root.page === "friends" ? "" : "home"
        currentInstanceId: root.page === "instance" ? root.openedInstanceId : ""
        recentModel: root.recentModel
        pickOnTap: root.page === "home"
        onItemActivated: (id) => root.page = id
        onRecentSelectRequested: (id) => root.selectedId = id
        onRecentActivated: (id) => root.page === "home" ? root.openInstanceSettings(id) : root.openInstance(id)
        onRecentPlayRequested: (id) => root.launch(id)
        onNewInstanceRequested: root.openNewInstance()

        accountName: root.shellValue("accountName", "")
        accountKind: root.shellValue("accountKind", "")
        accountAvatarSource: root.shellValue("accountFace", "")
        accountsController: root.accountsController
        onOpenAccountsRequested: root.page = "accounts"
    }

    // The open instance's name, read from the instance page model's row.
    property string openedName: ""
    Repeater {
        model: root.instancePageModel
        delegate: Item {
            required property string name
            visible: false
            onNameChanged: root.openedName = name
            Component.onCompleted: root.openedName = name
        }
    }

    TopBar {
        id: topBar
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.shellBarHeight
        leftReserve: Math.max(0, root.windowControlsInset - sidebar.width)
        sideMargin: root.pageSideMargin
        title: root.page === "settings" ? qsTr("Settings")
             : root.page === "discover" ? qsTr("Discover")
             : root.page === "accounts" ? qsTr("Accounts")
             : root.page === "friends" ? qsTr("Friends")
             : root.page === "instance" ? (instancePage && instancePage.browsing ? qsTr("Add content") : root.openedName)
             : qsTr("Home")
        iconName: root.page === "settings" ? "settings"
                : root.page === "discover" ? "compass"
                : root.page === "accounts" ? "user"
                : root.page === "friends" ? "users"
                : "home"
        trail: root.page !== "instance" ? []
             : instancePage && instancePage.browsing ? [qsTr("Home"), root.openedName] : [qsTr("Home")]
        onTrailActivated: (index) => {
            if (index === 0)
                root.page = "home"
            else
                instancePage.leaveBrowse()
        }
        // The title keeps to the page's column; the toggle stands over the panel.
        rightReserve: friendsPanel.width
        // Home only: the big servers to join in one click, against the right of the page's column.
        middle: [
            QuickJoinBar {
                visible: root.page === "home" && root.featuredList.length > 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, parent.width - Theme.space.xl)
                servers: root.featuredList
                pinger: root.serverPinger
                friends: root.friendsController ? root.friendsController.friends : []
                joinFlow: serverJoin
                canInvite: Format.friendsPhase(root.friendsController) === "ready"
                onJoinRequested: (server) => serverJoin.join(server)
                onJoinWithRequested: (server) => serverJoin.joinWithAnother(server)
                onInviteRequested: (server) => invitePicker.openFor(server)
            }
        ]
        trailing: [
            FriendsPanelToggle {
                visible: !!root.friendsController
                open: root.friendsPanelOpen
                // Counts only while the list is ready.
                onlineCount: Format.friendsPhase(root.friendsController) === "ready"
                             ? root.friendsController.onlineCount : 0
                requestCount: Format.friendsPhase(root.friendsController) === "ready"
                              ? root.friendsController.incomingCount : 0
                surround: root.shellColor
                onClicked: root.setFriendsPanelOpen(!root.friendsPanelOpen)
            }
        ]
    }

    FriendsSidePanel {
        id: friendsPanel
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        open: root.friendsPanelShown
        expanded: root.friendsPanelExpanded
        expandedWidth: root.friendsPanelWidth
        roomWidth: root.width - sidebar.width
        controller: root.friendsController
        places: root.friendPlaces
        joinFlow: friendJoin
        myFace: root.shellValue("accountFace", "")
        onExpandedRequested: (expanded) => root.setFriendsPanelExpanded(expanded)
        onWidthCommitted: (panelWidth) => root.setFriendsPanelWidth(panelWidth)
        onFriendsRequested: (tab) => root.openFriends(tab)
        onNavigateRequested: (target) => root.navigate(target)
        onJoinRequested: (friend) => friendJoin.join(friend)
    }

    // Clipped; the fill runs past the right and, without a play bar, the bottom
    // edge, so only the corners against the shell round off.
    Item {
        id: panel
        anchors.left: sidebar.right
        anchors.right: friendsPanel.left
        anchors.top: topBar.bottom
        anchors.bottom: playDock.visible ? playDock.top : parent.bottom
        clip: true

        readonly property int overhangBottom: playDock.visible ? 0 : root.panelRadius

        Rectangle {
            id: panelFill
            width: parent.width + root.panelRadius
            height: parent.height + panel.overhangBottom
            radius: root.panelRadius
            color: root.skyLook ? "transparent" : Theme.palette.canvas
        }

        ColumnLayout {
            anchors.fill: parent
            // Inside the hairline, which the matte redraws over the page.
            anchors.leftMargin: 1
            anchors.topMargin: 1
            anchors.bottomMargin: panel.overhangBottom > 0 ? 0 : 1
            spacing: 0

            // Fades the old page out, swaps StackLayout's currentIndex at the
            // invisible midpoint, then fades the new page in.
            Item {
                id: pageHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0

                readonly property var pageOrder: ["home", "settings", "discover", "instance", "accounts", "friends"]
                // Updated at the midpoint (pageTransition), not from root.page, so
                // the bar never changes while the old page is still fading.
                property bool dockVisible: root.dockVisibleFor(root.page)

                transform: Translate { id: pageSlide }

                SequentialAnimation {
                    id: pageTransition
                    property int nextIndex: 0
                    NumberAnimation { target: pageHost; property: "opacity"; to: 0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                    PropertyAction { target: pageStack; property: "currentIndex"; value: pageTransition.nextIndex }
                    PropertyAction { target: pageHost; property: "dockVisible"; value: root.dockVisibleFor(pageHost.pageOrder[pageTransition.nextIndex]) }
                    ScriptAction {
                        script: {
                            if (root.focusHomeSearch && root.page === "home")
                                homePage.focusSearch()
                            root.focusHomeSearch = false
                        }
                    }
                    ParallelAnimation {
                        NumberAnimation { target: pageHost; property: "opacity"; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                        NumberAnimation { target: pageSlide; property: "y"; from: Theme.space.sm; to: 0; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                    }
                }

                Connections {
                    target: root
                    function onPageChanged() {
                        pageTransition.nextIndex = pageHost.pageOrder.indexOf(root.page)
                        pageTransition.restart()
                    }
                }

                // Under the pages, so anything a page draws covers it.
                Loader {
                    anchors.fill: parent
                    active: SettingsStore.bool("TheCat")
                    source: active ? "qrc:/qt/qml/Kite/Components/CatPicture.qml" : ""
                }

                StackLayout {
                    id: pageStack
                    anchors.fill: parent
                    Component.onCompleted: currentIndex = pageHost.pageOrder.indexOf(root.page)

                    HomePage {
                        id: homePage
                        recentModel: root.recentModel
                        instanceModel: root.instanceModel
                        recentWorldsModel: root.shellValue("recentWorlds")
                        news: root.shellValue("news")
                        heroModel: root.heroModel
                        sectionModelFor: root.sectionModel
                        selectedId: root.selectedId
                        library: root.shellValue("library")
                        launchOptionsFor: root.launchOptions
                        exportControllerFor: root.exportController
                        shortcutControllerFor: root.shortcutController
                        shareCodes: root.shareCodes
                        msaSignedIn: root.shellValue("accountKind") === "Microsoft"
                        friends: root.friendsController
                        friendPlaces: root.friendPlaces
                        joinFlow: friendJoin
                        friendsInPanel: root.friendsPanelShown
                        onFriendsRequested: (tab) => root.openFriends(tab)
                        onJoinRequested: (friend) => friendJoin.join(friend)

                        onLaunchRequested: (id) => root.launch(id)
                        onStopRequested: (id) => root.call("killInstance", id)
                        onOpenInstanceRequested: (id) => root.openInstanceSettings(id)
                        onSelectRequested: (id) => root.selectedId = id
                        onSignInRequested: root.page = "accounts"
                        onFolderRequested: (id) => root.call("openInstanceFolder", id)
                        onRenameRequested: (id, name) => root.askRename(id, name)
                        onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                        onGroupRequested: (id, group) => root.askGroup(id, group)
                        onDeleteRequested: (id, name) => root.askDelete(id, name)
                        onOpenInstanceWorldsRequested: (id) => {
                            root.openInstance(id)
                            instancePage.tab = "worlds"
                        }
                        onCreateRequested: root.openNewInstance()
                        onDiscoverRequested: root.page = "discover"
                    }

                    PageSlot {
                        id: settingsLoader
                        page: "settings"
                        current: root.page
                        sourceComponent: SettingsPage {
                                languages: root.shellValue("languages")
                                selectLanguage: function (key) { root.shell.selectLanguage(key) }
                                pluginSurfaces: root.pluginSurfaces(0, "")
                                systemMemoryMiB: root.shellValue("systemMemoryMiB", 8192)
                                about: root.shellValue("about")
                                pluginList: root.shellResult("pluginList")
                                launcherLogs: root.shellValue("launcherLogs")
                                friends: root.friendsController
                                shareCodes: root.shareCodes
                                friendsAccountName: root.shellValue("accountKind") === "Microsoft"
                                                    ? root.shellValue("accountName", "") : ""
                                onOpenPathRequested: (path) => root.call("openPath", path)
                        }
                    }

                    PageSlot {
                        id: discoverLoader
                        page: "discover"
                        current: root.page
                        sourceComponent: DiscoverPage {
                                shown: discoverLoader.StackLayout.isCurrentItem
                                model: root.shellValue("modpackModel")
                                curseForgeModel: root.shellValue("curseForgeModpackModel")
                                ftbModel: root.shellValue("ftbModpackModel")
                                atlModel: root.shellValue("atlModpackModel")
                                technicModel: root.shellValue("technicModpackModel")
                                installer: function (projectId, versionId, name, group) {
                                    return root.shell.installModpack(projectId, versionId, name, group)
                                }
                                onShowInstanceRequested: (id) => {
                                    root.page = "home"
                                    if (id.length > 0)
                                        root.selectedId = id
                                }
                                onOtherPlatformsRequested: root.openNewInstance("import")
                        }
                    }

                    PageSlot {
                        id: instanceLoader
                        page: "instance"
                        current: root.page
                        sourceComponent: InstancePage {
                                headerModel: root.instancePageModel
                                details: root.openedDetails
                                shell: root.shell
                                systemMemoryMiB: root.shellValue("systemMemoryMiB", 8192)
                                accountName: root.shellValue("accountName", "")
                                accountKind: root.shellValue("accountKind", "")
                                accountAvatarSource: root.shellValue("accountFace", "")
                                accountsController: root.accountsController
                                onOpenAccountsRequested: root.page = "accounts"
                                onBackRequested: root.page = "home"
                                onOpenInstanceRequested: (id) => root.openInstance(id)
                                onPlayRequested: (id) => root.launch(id)
                                onStopRequested: (id) => root.call("killInstance", id)
                                manageable: true
                                library: root.shellValue("library")
                                onRenameRequested: (id, name) => root.askRename(id, name)
                                onIconRequested: (id, iconKey) => root.askIcon(id, iconKey)
                                onGroupRequested: (id, group) => root.askGroup(id, group)
                                onDeleteRequested: (id, name) => root.askDelete(id, name)
                                // root.call() forwards one argument only.
                                onJoinServerRequested: (id, address) => { if (root.shell) root.shell.joinServer(id, address) }
                                onInviteRequested: (name, address) => invitePicker.openFor({ name: name, address: address })
                                pluginSurfacesFor: root.pluginSurfaces
                                onOpenPathRequested: (path) => root.call("openPath", path)
                                exportControllerFor: root.exportController
                                shortcutControllerFor: root.shortcutController
                                launchOptionsFor: root.launchOptions
                        }
                    }

                    PageSlot {
                        id: accountsLoader
                        page: "accounts"
                        current: root.page
                        sourceComponent: AccountsPage {
                                controller: root.accountsController
                                skin3DAvailable: root.skin3DAvailable
                        }
                    }

                    PageSlot {
                        id: friendsLoader
                        page: "friends"
                        current: root.page
                        sourceComponent: FriendsPage {
                                shown: friendsLoader.StackLayout.isCurrentItem
                                controller: root.friendsController
                                places: root.friendPlaces
                                myFace: root.shellValue("accountFace", "")
                                joinFlow: friendJoin
                                onNavigateRequested: (target) => root.navigate(target)
                                onJoinRequested: (friend) => friendJoin.join(friend)
                                onMessageRequested: (uuid) => root.openFriendChat(uuid)
                        }
                    }
                }

            }
        }

        // Over the page: the shell colour outside the rounded corners, so
        // edge-to-edge content cannot square them off, plus the hairline.
        Item {
            width: panelFill.width
            height: panelFill.height

            CornerMatte {
                visible: !root.skyLook
                matte: root.shellColor
                cornerRadius: root.panelRadius
                outline: Theme.palette.border
            }
        }
    }

    // Part of the frame, not an overlay: the page area shrinks by its height.
    // visible follows pageHost.dockVisible so it changes at the fade midpoint.
    PlayDock {
        id: playDock
        surround: root.shellColor
        anchors.left: sidebar.right
        anchors.right: friendsPanel.left
        anchors.bottom: parent.bottom
        height: implicitHeight
        sideMargin: root.pageSideMargin
        visible: pageHost.dockVisible && !root.dockSuppressed && !root.dockHeldByHero
        rowModel: root.heroModel
        instanceModel: root.instanceModel
        recentModel: root.recentModel
        selectedId: root.selectedId
        onSelectRequested: (id) => root.selectedId = id
        onCreateRequested: root.openNewInstance()
        onLaunchRequested: (id) => root.launch(id)
        onStopRequested: (id) => root.call("killInstance", id)
        onCancelRequested: (id) => root.call("killInstance", id)
        launchOptionsFor: root.launchOptions
    }

    // Dialogs are built on first use (root.dialog()); a closed Dialog still
    // builds its whole body.
    Loader {
        id: newInstanceLoader
        active: false
        sourceComponent: NewInstanceDialog {
            iconsModel: root.shellValue("iconsModel")
            shell: root.shell
            shareCodes: root.shareCodes
            onCreated: root.page = "home"
            onOpenInstanceRequested: (id) => root.navigate("instance/" + id + "/content")
        }
    }

    Loader {
        id: crashWindowLoader
        active: false
        sourceComponent: CrashDoctorDialog {
            shell: root.shell
            toast: toast
            onTabRequested: (id, tab) => {
                root.openInstance(id)
                if (instancePage)
                    instancePage.tab = tab
            }
            onLogLineRequested: (id, line) => {
                root.openInstance(id)
                if (instancePage)
                    instancePage.showCrashLine(line)
            }
            onCrashReportRequested: (id, name) => {
                root.openInstance(id)
                if (instancePage)
                    instancePage.showCrashReport(name)
            }
            onUploadRequested: (id) => {
                root.openInstance(id)
                if (instancePage)
                    instancePage.uploadCrashLog()
            }
            onPlayRequested: (id) => root.launch(id)
            onFolderRequested: (path) => root.call("openPath", path)
        }
    }

    Loader {
        id: renameLoader
        active: false
        sourceComponent: PromptDialog {
            property string targetId
            title: qsTr("Rename instance")
            confirmText: qsTr("Rename")
            onSubmitted: (text) => {
                if (root.shell.renameInstance(targetId, text))
                    close()
                else
                    error = qsTr("That name cannot be used.")
            }
        }
    }

    Loader {
        id: groupLoader
        active: false
        sourceComponent: PromptDialog {
            property string targetId
            title: qsTr("Move to group")
            label: qsTr("Type a new group or pick an existing one.")
            placeholder: qsTr("No group")
            confirmText: qsTr("Move")
            allowEmpty: true
            onSubmitted: (text) => {
                root.shell.setInstanceGroup(targetId, text)
                close()
            }
        }
    }

    Loader {
        id: deleteLoader
        active: false
        sourceComponent: ConfirmDialog {
            property string targetId
            title: qsTr("Delete instance")
            confirmText: qsTr("Delete instance")
            onConfirmed: {
                if (root.shell.deleteInstance(targetId)) {
                    if (root.selectedId === targetId)
                        root.selectedId = ""
                    if (root.page === "instance" && root.openedInstanceId === targetId)
                        root.page = "home"
                    // Undo only when it went to the trash.
                    var library = root.shell.library
                    if (library && library.canUndoDelete) {
                        var trashedName = library.lastDeletedName
                        toast.show(qsTr("Instance deleted."), "success", qsTr("Undo"), function () {
                            if (library.undoDelete())
                                toast.show(qsTr("“%1” is back in your library.").arg(trashedName), "success")
                            else
                                toast.show(qsTr("“%1” could not be put back. It is still in the trash.").arg(trashedName), "danger")
                        })
                    } else {
                        toast.show(qsTr("Instance deleted."), "success")
                    }
                } else {
                    toast.show(qsTr("The instance could not be deleted. Is it running, or are its files in use?"), "danger")
                }
            }
        }
    }

    Connections {
        target: root.friendsController
        function onActionFinished(action, ok, message) {
            // A failed "Try again" is already shown on the page or panel.
            if (action === "refresh" && root.friendsController.state === "error")
                return
            if (message.length > 0)
                toast.show(message, ok ? "success" : "danger")
        }
    }

    FriendJoinFlow {
        id: friendJoin
        shell: root.shell
        selectedId: root.selectedId
        toast: toast
        serverFlow: serverJoin
        featured: root.featuredList
        pinger: root.serverPinger
        onCreateInstanceRequested: (version) => root.createInstanceOn(version)
    }

    FriendInvitePicker {
        id: invitePicker
        controller: root.friendsController
        places: root.friendPlaces
    }

    Connections {
        target: root.shareCodes
        function onActionFinished(action, ok, message) {
            // The dialogs show their own; a finished instance's left-out content has no dialog left to say it.
            if ((action === "createInstance" || action === "deleteCode") && message.length > 0)
                toast.show(message, ok ? "success" : "danger")
        }
    }

    ServerJoinFlow {
        id: serverJoin
        shell: root.shell
        selectedId: root.dockInstanceId
        toast: toast
        onCreateInstanceRequested: (version) => version.length > 0 ? root.createInstanceOn(version) : root.openNewInstance()
    }

    Loader {
        id: iconLoader
        active: false
        sourceComponent: IconPickerDialog {
            property string targetId
            iconsModel: root.shellValue("iconsModel")
            onPicked: (key) => root.shell.setInstanceIcon(targetId, key)
            onOpenFolderRequested: root.call("openPath", root.shell.iconsDir || "icons")
        }
    }

    // Over the page while files are dragged across an instance's page.
    DropOverlay {
        anchors.fill: panel
        radius: root.panelRadius
        visible: (windowDrop.containsDrag || windowDrop.previewing) && windowDrop.onInstance
        rows: windowDrop.rows
        unknown: windowDrop.unknown
    }

    Toast {
        id: toast
        anchors.bottomMargin: (playDock.visible ? playDock.height : 0) + Theme.space.xl
    }

    UiRequestDialog {
        id: uiRequests
        host: root.uiHost
        // Until this exists, uiHost() is the headless host and declines every question (Application.cpp HeadlessUiHost).
        Component.onCompleted: if (host) host.setPresenterReady(true)
        Component.onDestruction: if (host) host.setPresenterReady(false)
    }

    BusyOverlay {
        busy: !!root.uiHost && root.uiHost.busy
        text: root.uiHost ? root.uiHost.busyText : ""
        skipLabel: root.uiHost && root.uiHost.busySkipLabel !== undefined ? root.uiHost.busySkipLabel : ""
        onSkipRequested: if (root.uiHost) root.uiHost.skipBusy()
    }

    // Built only when a first-run step exists or the "onboarding" route asks.
    // z: the view's own z orders only inside this Loader.
    Loader {
        id: onboardingLoader
        active: false
        anchors.fill: parent
        z: 800
        sourceComponent: OnboardingView {
            shell: root.shell
            topInset: root.windowControlsInset > 0 ? root.shellBarHeight : 0
            onSignInRequested: {
                root.page = "accounts"
                accountsPage.startMicrosoftLogin()
            }
            onCreateInstanceRequested: root.openNewInstance()
            onDiscoverRequested: root.page = "discover"
            Component.onCompleted: start()
        }
    }
}
